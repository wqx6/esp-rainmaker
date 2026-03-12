/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <cJSON.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <app_matter_device_manager.h>
#include <esp_matter_controller_subscribe_command.h>
#include <esp_matter_controller_utils.h>
#include <esp_matter_core.h>
#include <matter_attr_report.h>
#include <matter_controller_cmd_resp.h>
#include <matter_device.h>

using namespace esp_matter;
using namespace esp_matter::controller;

#define TAG "matter_attr_rpt"

/* Filter: ignore endpoint 0, global attributes 0xFFF8-0xFFFD, cluster 0x1D */
#define MATTER_ATTR_FILTER_EP0            0
#define MATTER_ATTR_FILTER_GLOBAL_ATTR_MIN 0xFFF8
#define MATTER_ATTR_FILTER_GLOBAL_ATTR_MAX 0xFFFD
#define MATTER_ATTR_FILTER_CLUSTER_1D      0x1D

#define MATTER_ATTR_VALUE_MAX_LEN         384
#define MATTER_ATTR_QUEUE_SIZE            32
#define MATTER_ATTR_MAX_REMOVED_PER_UPDATE 32
#define MATTER_ATTR_TASK_STACK            8192
#define MATTER_ATTR_TASK_PRIO             5

typedef enum {
    ATTR_REPORT_MSG_ATTRIBUTE_DATA = 0,
    ATTR_REPORT_MSG_SUBSCRIPTION_TERMINATED,
    ATTR_REPORT_MSG_RESUBSCRIBE_RETRY,
    ATTR_REPORT_MSG_SUBSCRIBE_CONNECT_FAILED,
} attr_report_msg_type_t;

typedef struct {
    attr_report_msg_type_t msg_type;
    uint64_t node_id;
    uint16_t endpoint_id;
    uint32_t cluster_id;
    uint32_t attribute_id;
    char value[MATTER_ATTR_VALUE_MAX_LEN];
} attr_report_msg_t;

#define MATTER_ATTR_QUEUE_ITEM_SIZE       sizeof(attr_report_msg_t)

#define MATTER_ATTR_FIB_MAX_SEC 3600u
/** First resubscribe delay after going offline (then Fibonacci growth, capped by MATTER_ATTR_FIB_MAX_SEC). */
#define MATTER_ATTR_FIB_FIRST_SEC 60u

typedef struct node_state {
    uint64_t node_id;
    char rainmaker_node_id[ESP_RAINMAKER_NODE_ID_MAX_LEN];
    cJSON *root; /* endpoints -> 0xEP -> clusters -> servers -> 0xCID -> attributes -> 0xAID -> value */
    bool online; /* true while subscription is active, false when subscription is terminated */
    uint32_t fib_prev_sec;       /* Fibonacci backoff; first delay MATTER_ATTR_FIB_FIRST_SEC (e.g. 60,60,120,...) */
    uint32_t fib_cur_sec;
    esp_timer_handle_t resubscribe_timer;
    struct node_state *next;
} node_state_t;

static QueueHandle_t s_attr_report_queue = NULL;
static TaskHandle_t s_attr_report_task = NULL;
static SemaphoreHandle_t s_state_mutex = NULL;
static node_state_t *s_node_states = NULL;
static esp_rmaker_param_t *s_attributes_param = NULL;
static uint32_t s_last_report_hash = 0;
static bool s_have_last_report_hash = false;

static uint32_t hash_string(const char *str)
{
    uint32_t h = 5381;
    if (!str) {
        return 0;
    }
    while (*str) {
        h = ((h << 5) + h) + (uint8_t)*str++;
    }
    return h;
}

/* Sort object keys lexicographically so {"1":1,"2":2} and {"2":2,"1":1} canonicalize identically. */
static int cmp_cjson_object_entry(const void *a, const void *b)
{
    const cJSON *const *ja = (const cJSON *const *)a;
    const cJSON *const *jb = (const cJSON *const *)b;
    const char *sa = (*ja)->string;
    const char *sb = (*jb)->string;
    if (!sa && !sb) {
        return 0;
    }
    if (!sa) {
        return -1;
    }
    if (!sb) {
        return 1;
    }
    return strcmp(sa, sb);
}

/**
 * Deep copy with all JSON objects having keys in sorted order (arrays keep element order).
 * Used so content hash is order-insensitive for object keys.
 */
static cJSON *cjson_canonicalize(const cJSON *item)
{
    if (!item) {
        return NULL;
    }
    if (cJSON_IsObject(item)) {
        int count = 0;
        for (cJSON *c = item->child; c; c = c->next) {
            count++;
        }
        if (count == 0) {
            return cJSON_CreateObject();
        }
        cJSON **entries = (cJSON **)calloc((size_t)count, sizeof(cJSON *));
        if (!entries) {
            return NULL;
        }
        int i = 0;
        for (cJSON *c = item->child; c; c = c->next) {
            entries[i++] = c;
        }
        qsort(entries, (size_t)count, sizeof(cJSON *), cmp_cjson_object_entry);
        cJSON *out = cJSON_CreateObject();
        if (!out) {
            free(entries);
            return NULL;
        }
        for (i = 0; i < count; i++) {
            cJSON *child = cjson_canonicalize(entries[i]);
            if (!child) {
                cJSON_Delete(out);
                free(entries);
                return NULL;
            }
            cJSON_AddItemToObject(out, entries[i]->string, child);
        }
        free(entries);
        return out;
    }
    if (cJSON_IsArray(item)) {
        cJSON *out = cJSON_CreateArray();
        if (!out) {
            return NULL;
        }
        cJSON *c;
        cJSON_ArrayForEach(c, item)
        {
            cJSON *child = cjson_canonicalize(c);
            if (!child) {
                cJSON_Delete(out);
                return NULL;
            }
            cJSON_AddItemToArray(out, child);
        }
        return out;
    }
    return cJSON_Duplicate(item, 1);
}

static bool should_ignore_attribute(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id)
{
    if (endpoint_id == MATTER_ATTR_FILTER_EP0) {
        return true;
    }
    if (attribute_id >= MATTER_ATTR_FILTER_GLOBAL_ATTR_MIN && attribute_id <= MATTER_ATTR_FILTER_GLOBAL_ATTR_MAX) {
        return true;
    }
    if (cluster_id == MATTER_ATTR_FILTER_CLUSTER_1D) {
        return true;
    }
    return false;
}

/** True if key is legacy decimal only (e.g. "1", "98") — not "0x1". */
static bool is_legacy_decimal_key(const char *key)
{
    if (!key || !*key) {
        return false;
    }
    if (key[0] == '0' && (key[1] == 'x' || key[1] == 'X')) {
        return false;
    }
    for (const char *p = key; *p; p++) {
        if (*p < '0' || *p > '9') {
            return false;
        }
    }
    return true;
}

/** Drop pre-change layout (endpoint -> cluster -> attr) so we can rebuild Matter-Devices shape. */
static void strip_legacy_attr_root(cJSON *root)
{
    if (!root || !cJSON_IsObject(root)) {
        return;
    }
    for (cJSON *c = root->child; c != NULL; c = c->next) {
        if (c->string && is_legacy_decimal_key(c->string)) {
            while (root->child) {
                cJSON_Delete(cJSON_DetachItemViaPointer(root, root->child));
            }
            return;
        }
    }
}

/** Recursively rename object keys from decimal strings to 0x-prefixed uppercase hex (Matter field ids). */
static void cjson_hexify_matter_keys(cJSON *item)
{
    if (!item) {
        return;
    }
    if (cJSON_IsObject(item)) {
        cJSON *child = item->child;
        while (child) {
            cJSON *next = child->next;
            const char *key = child->string;
            if (key && key[0] != '\0') {
                bool need_hex = false;
                if (strncmp(key, "0x", 2) != 0 && strncmp(key, "0X", 2) != 0) {
                    char *end = NULL;
                    (void)strtoul(key, &end, 10);
                    if (end && end > key && *end == '\0') {
                        need_hex = true;
                    }
                }
                if (need_hex) {
                    char new_key[32];
                    unsigned long v = strtoul(key, NULL, 10);
                    snprintf(new_key, sizeof(new_key), "0x%lX", v);
                    cJSON *detached = cJSON_DetachItemViaPointer(item, child);
                    cjson_hexify_matter_keys(detached);
                    cJSON_AddItemToObject(item, new_key, detached);
                } else {
                    cjson_hexify_matter_keys(child);
                }
            }
            child = next;
        }
    } else if (cJSON_IsArray(item)) {
        cJSON *el = NULL;
        cJSON_ArrayForEach(el, item)
        {
            cjson_hexify_matter_keys(el);
        }
    }
}

static node_state_t *find_node_state(uint64_t node_id)
{
    for (node_state_t *n = s_node_states; n != NULL; n = n->next) {
        if (n->node_id == node_id) {
            return n;
        }
    }
    return NULL;
}

static void free_node_state(node_state_t *ns)
{
    if (ns->resubscribe_timer) {
        esp_timer_stop(ns->resubscribe_timer);
        esp_timer_delete(ns->resubscribe_timer);
        ns->resubscribe_timer = NULL;
    }
    if (ns->root) {
        cJSON_Delete(ns->root);
        ns->root = NULL;
    }
    free(ns);
}

static node_state_t *create_node_state(uint64_t node_id, const char *rainmaker_node_id)
{
    node_state_t *ns = (node_state_t *)calloc(1, sizeof(node_state_t));
    if (!ns) {
        return NULL;
    }
    ns->node_id = node_id;
    if (rainmaker_node_id) {
        strncpy(ns->rainmaker_node_id, rainmaker_node_id, sizeof(ns->rainmaker_node_id) - 1);
        ns->rainmaker_node_id[sizeof(ns->rainmaker_node_id) - 1] = '\0';
    }
    ns->root = cJSON_CreateObject();
    if (!ns->root) {
        free(ns);
        return NULL;
    }
    ns->online = false; /* set true in attr_report_task when first attribute data arrives */
    ns->fib_prev_sec = 0;
    ns->fib_cur_sec = MATTER_ATTR_FIB_FIRST_SEC;
    ns->resubscribe_timer = NULL;
    return ns;
}

static void reset_fib_backoff(node_state_t *ns)
{
    ns->fib_prev_sec = 0;
    ns->fib_cur_sec = MATTER_ATTR_FIB_FIRST_SEC;
}

static void stop_resubscribe_timer(node_state_t *ns)
{
    if (ns && ns->resubscribe_timer) {
        esp_timer_stop(ns->resubscribe_timer);
    }
}

static void resubscribe_timer_cb(void *arg)
{
    node_state_t *ns = (node_state_t *)arg;
    if (!ns || !s_attr_report_queue) {
        return;
    }
    attr_report_msg_t refresh = {};
    refresh.msg_type = ATTR_REPORT_MSG_RESUBSCRIBE_RETRY;
    refresh.node_id = ns->node_id;
    if (xQueueSend(s_attr_report_queue, &refresh, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Attr report queue full, drop resubscribe retry for 0x%llX",
                 (unsigned long long)ns->node_id);
    }
}

static esp_err_t ensure_resubscribe_timer(node_state_t *ns)
{
    if (ns->resubscribe_timer) {
        return ESP_OK;
    }
    esp_timer_create_args_t args = {
        .callback = &resubscribe_timer_cb,
        .arg = ns,
        .name = "mt_attr_rs",
    };
    return esp_timer_create(&args, &ns->resubscribe_timer);
}

/* Requires s_state_mutex held. Schedules next one-shot retry; advances Fibonacci state. */
static void schedule_resubscribe_attempt(node_state_t *ns)
{
    if (!ns) {
        return;
    }
    ESP_LOGI(TAG, "Scheduling resubscribe attempt for 0x%llX", (unsigned long long)ns->node_id);
    if (ensure_resubscribe_timer(ns) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to create resubscribe timer for 0x%llX", (unsigned long long)ns->node_id);
        return;
    }
    uint32_t delay_sec = ns->fib_cur_sec;
    if (delay_sec > MATTER_ATTR_FIB_MAX_SEC) {
        delay_sec = MATTER_ATTR_FIB_MAX_SEC;
    }
    esp_timer_stop(ns->resubscribe_timer);
    esp_err_t err = esp_timer_start_once(ns->resubscribe_timer, (uint64_t)delay_sec * 1000000ULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_timer_start_once failed for 0x%llX: %s", (unsigned long long)ns->node_id,
                 esp_err_to_name(err));
        return;
    }
    uint32_t nxt = ns->fib_prev_sec + ns->fib_cur_sec;
    if (nxt < ns->fib_cur_sec || nxt > MATTER_ATTR_FIB_MAX_SEC) {
        nxt = MATTER_ATTR_FIB_MAX_SEC;
    }
    ns->fib_prev_sec = ns->fib_cur_sec;
    if (ns->fib_prev_sec > MATTER_ATTR_FIB_MAX_SEC) {
        ns->fib_prev_sec = MATTER_ATTR_FIB_MAX_SEC;
    }
    ns->fib_cur_sec = nxt;
}

static esp_err_t send_wildcard_subscribe(uint64_t node_id);

/*
 * Update tree at endpoints/0xEP/clusters/servers/0xCID/attributes/0xAID.
 * TLV decode uses JSON text; parse when possible. Nested object keys are hexified to 0x-prefixed ids.
 */
static void update_attr_tree(cJSON *root, uint16_t endpoint_id, uint32_t cluster_id,
                             uint32_t attribute_id, const char *value)
{
    strip_legacy_attr_root(root);

    char ep_key[24];
    char cluster_key[24];
    char attr_key[24];
    snprintf(ep_key, sizeof(ep_key), "0x%X", (unsigned)endpoint_id);
    snprintf(cluster_key, sizeof(cluster_key), "0x%" PRIX32, cluster_id);
    snprintf(attr_key, sizeof(attr_key), "0x%" PRIX32, attribute_id);

    cJSON *ep_obj = cJSON_GetObjectItem(root, ep_key);
    if (!ep_obj) {
        ep_obj = cJSON_CreateObject();
        if (ep_obj) {
            cJSON_AddItemToObject(root, ep_key, ep_obj);
        } else {
            return;
        }
    }
    cJSON *clusters_obj = cJSON_GetObjectItem(ep_obj, "clusters");
    if (!clusters_obj) {
        clusters_obj = cJSON_CreateObject();
        if (!clusters_obj) {
            return;
        }
        cJSON_AddItemToObject(ep_obj, "clusters", clusters_obj);
    }
    cJSON *servers_obj = cJSON_GetObjectItem(clusters_obj, "servers");
    if (!servers_obj) {
        servers_obj = cJSON_CreateObject();
        if (!servers_obj) {
            return;
        }
        cJSON_AddItemToObject(clusters_obj, "servers", servers_obj);
    }
    cJSON *cluster_wrap = cJSON_GetObjectItem(servers_obj, cluster_key);
    if (!cluster_wrap) {
        cluster_wrap = cJSON_CreateObject();
        if (!cluster_wrap) {
            return;
        }
        cJSON_AddItemToObject(servers_obj, cluster_key, cluster_wrap);
    }
    cJSON *attr_obj = cJSON_GetObjectItem(cluster_wrap, "attributes");
    if (!attr_obj) {
        attr_obj = cJSON_CreateObject();
        if (!attr_obj) {
            return;
        }
        cJSON_AddItemToObject(cluster_wrap, "attributes", attr_obj);
    }
    cJSON_Delete(cJSON_DetachItemFromObject(attr_obj, attr_key));

    cJSON *parsed = NULL;
    if (value && value[0] != '\0') {
        parsed = cJSON_Parse(value);
    }
    if (parsed) {
        cjson_hexify_matter_keys(parsed);
        cJSON_AddItemToObject(attr_obj, attr_key, parsed);
    } else {
        cJSON_AddItemToObject(attr_obj, attr_key, cJSON_CreateString(value ? value : ""));
    }
}

/* Build and report aggregated Matter devices map via RainMaker param (`esp.param.matter-attributes`). */
static void publish_all_nodes_report(void)
{
    if (!s_attributes_param) {
        return;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return;
    }
    for (node_state_t *ns = s_node_states; ns != NULL; ns = ns->next) {
        if (!ns->root) {
            continue;
        }
        cJSON *wrapper = cJSON_CreateObject();
        if (!wrapper) {
            continue;
        }
        char node_key[32];
        snprintf(node_key, sizeof(node_key), "%016llx", (unsigned long long)ns->node_id);
        cJSON_AddItemToObject(wrapper, "rainmaker_node_id", cJSON_CreateString(ns->rainmaker_node_id));
        cJSON_AddItemToObject(wrapper, "online", cJSON_CreateBool(ns->online));
        cJSON_AddItemToObject(wrapper, "endpoints", cJSON_Duplicate(ns->root, 1));
        cJSON_AddItemToObject(root, node_key, wrapper);
    }
    cJSON *canonical = cjson_canonicalize(root);
    cJSON_Delete(root);
    if (!canonical) {
        return;
    }
    char *payload = cJSON_PrintUnformatted(canonical);
    cJSON_Delete(canonical);
    if (!payload) {
        return;
    }

    uint32_t h = hash_string(payload);
    if (s_have_last_report_hash && h == s_last_report_hash) {
        cJSON_free(payload);
        return;
    }
    s_last_report_hash = h;
    s_have_last_report_hash = true;

    esp_err_t err = esp_rmaker_param_update_and_report(s_attributes_param, esp_rmaker_obj(payload));
    cJSON_free(payload);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to report matter attributes param: %s", esp_err_to_name(err));
    }
}

static bool is_node_in_list(matter_device_t *list, uint64_t node_id)
{
    for (matter_device_t *d = list; d != NULL; d = d->next) {
        if (d->node_id == node_id) {
            return true;
        }
    }
    return false;
}

static void on_subscribe_connect_failure_cb(void *context, const chip::ScopedNodeId &peer_id, CHIP_ERROR error)
{
    subscribe_command *cmd = static_cast<subscribe_command *>(context);
    if (!cmd || !s_attr_report_queue) {
        return;
    }
    uint64_t node_id = cmd->get_node_id();
    attr_report_msg_t m = {};
    m.msg_type = ATTR_REPORT_MSG_SUBSCRIBE_CONNECT_FAILED;
    m.node_id = node_id;
    if (xQueueSend(s_attr_report_queue, &m, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Attr report queue full, drop connect-fail for 0x%llX", (unsigned long long)node_id);
    }
}

/* Called when subscription is terminated (device offline or subscription ended). */
static void on_subscribe_done_cb(uint64_t node_id, uint32_t subscription_id)
{
    (void)subscription_id;
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    node_state_t *ns = find_node_state(node_id);
    if (ns) {
        ns->online = false;
    }
    xSemaphoreGive(s_state_mutex);
    /* Post a sentinel message so the task publishes the updated report (with this node offline). */
    attr_report_msg_t refresh = {};
    refresh.msg_type = ATTR_REPORT_MSG_SUBSCRIPTION_TERMINATED;
    refresh.node_id = node_id;
    if (s_attr_report_queue) {
        xQueueSend(s_attr_report_queue, &refresh, 0);
    }
}

static void attr_report_task(void *arg)
{
    attr_report_msg_t msg;
    while (xQueueReceive(s_attr_report_queue, &msg, portMAX_DELAY) == pdTRUE) {
        if (msg.msg_type == ATTR_REPORT_MSG_SUBSCRIBE_CONNECT_FAILED) {
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            node_state_t *ns_cf = find_node_state(msg.node_id);
            if (ns_cf && !ns_cf->online) {
                ESP_LOGW(TAG, "Subscribe connect failed for 0x%llX, scheduling backoff retry",
                         (unsigned long long)msg.node_id);
                schedule_resubscribe_attempt(ns_cf);
            }
            xSemaphoreGive(s_state_mutex);
            continue;
        }
        if (msg.msg_type == ATTR_REPORT_MSG_RESUBSCRIBE_RETRY) {
            matter_device_t *dev_list = fetch_device_list();
            if (!dev_list || !is_node_in_list(dev_list, msg.node_id)) {
                if (dev_list) {
                    free_matter_device_list(dev_list);
                }
                xSemaphoreTake(s_state_mutex, portMAX_DELAY);
                node_state_t *ns_stop = find_node_state(msg.node_id);
                if (ns_stop) {
                    stop_resubscribe_timer(ns_stop);
                }
                xSemaphoreGive(s_state_mutex);
                continue;
            }
            free_matter_device_list(dev_list);

            uint64_t retry_node_id;
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            node_state_t *ns = find_node_state(msg.node_id);
            if (!ns) {
                xSemaphoreGive(s_state_mutex);
                continue;
            }
            if (ns->online) {
                stop_resubscribe_timer(ns);
                xSemaphoreGive(s_state_mutex);
                continue;
            }
            retry_node_id = ns->node_id;
            xSemaphoreGive(s_state_mutex);

            esp_err_t err = send_wildcard_subscribe(retry_node_id);

            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            ns = find_node_state(retry_node_id);
            if (ns && !ns->online) {
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "Resubscribe send_command failed for 0x%llX: %s",
                             (unsigned long long)retry_node_id, esp_err_to_name(err));
                    schedule_resubscribe_attempt(ns);
                }
                /* Online / fib reset only when attribute data arrives; connect failure uses connect_failure_cb. */
                publish_all_nodes_report();
            }
            xSemaphoreGive(s_state_mutex);
            continue;
        }

        if (msg.msg_type == ATTR_REPORT_MSG_SUBSCRIPTION_TERMINATED) {
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            node_state_t *nst = find_node_state(msg.node_id);
            if (nst) {
                cJSON *empty = cJSON_CreateObject();
                if (empty) {
                    cJSON_Delete(nst->root);
                    nst->root = empty;
                } else {
                    ESP_LOGW(TAG, "Failed to alloc empty attr root for 0x%llX", (unsigned long long)msg.node_id);
                }
                publish_all_nodes_report();
                schedule_resubscribe_attempt(nst);
            } else {
                publish_all_nodes_report();
            }
            xSemaphoreGive(s_state_mutex);
            continue;
        }

        if (msg.msg_type != ATTR_REPORT_MSG_ATTRIBUTE_DATA) {
            ESP_LOGW(TAG, "Unknown attr report msg_type %d", (int)msg.msg_type);
            continue;
        }

        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        node_state_t *ns = find_node_state(msg.node_id);
        if (!ns) {
            xSemaphoreGive(s_state_mutex);
            continue;
        }
        stop_resubscribe_timer(ns);
        reset_fib_backoff(ns);
        ns->online = true;
        update_attr_tree(ns->root, msg.endpoint_id, msg.cluster_id, msg.attribute_id, msg.value);
        publish_all_nodes_report();
        xSemaphoreGive(s_state_mutex);
    }
}

static void on_attribute_data_cb(uint64_t remote_node_id, const chip::app::ConcreteDataAttributePath &path,
                                 chip::TLV::TLVReader *data, const chip::app::StatusIB &status)
{
    if (should_ignore_attribute(path.mEndpointId, path.mClusterId, path.mAttributeId)) {
        return;
    }
    if (status.ToChipError() != CHIP_NO_ERROR) {
        return;
    }
    attr_report_msg_t msg = {};
    msg.msg_type = ATTR_REPORT_MSG_ATTRIBUTE_DATA;
    msg.node_id = remote_node_id;
    msg.endpoint_id = path.mEndpointId;
    msg.cluster_id = path.mClusterId;
    msg.attribute_id = path.mAttributeId;
    if (data) {
        matter_controller_decode_tlv_to_string(data, msg.value, sizeof(msg.value));
    } else {
        snprintf(msg.value, sizeof(msg.value), "null");
    }
    if (s_attr_report_queue && xQueueSend(s_attr_report_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Attr report queue full, drop node 0x%llX ep %u", (unsigned long long)remote_node_id,
                 path.mEndpointId);
    }
}

static esp_err_t send_wildcard_subscribe(uint64_t node_id)
{
    esp_matter::lock::ScopedChipStackLock chip_lock(portMAX_DELAY);
    subscribe_command *cmd = chip::Platform::New<subscribe_command>(
        node_id, 0xFFFF, 0xFFFFFFFF, 0xFFFFFFFF, SUBSCRIBE_ATTRIBUTE, 0, 60, true, on_attribute_data_cb, nullptr, nullptr,
        on_subscribe_done_cb, on_subscribe_connect_failure_cb);
    if (!cmd) {
        return ESP_ERR_NO_MEM;
    }
    /* send_command returns ESP_OK when session setup was queued; failure is async via connect_failure_cb.
     * On immediate ESP_FAIL, subscribe_command already deletes itself. */
    return cmd->send_command();
}

esp_err_t matter_attr_report_init(esp_rmaker_param_t *attributes_param)
{
    s_attributes_param = attributes_param;
    if (!s_attributes_param) {
        ESP_LOGW(TAG, "Matter attributes param is NULL; attribute snapshot will not be reported");
    }

    if (s_attr_report_queue != NULL) {
        return ESP_OK;
    }
    s_state_mutex = xSemaphoreCreateMutex();
    if (!s_state_mutex) {
        return ESP_ERR_NO_MEM;
    }
    s_attr_report_queue = xQueueCreate(MATTER_ATTR_QUEUE_SIZE, MATTER_ATTR_QUEUE_ITEM_SIZE);
    if (!s_attr_report_queue) {
        vSemaphoreDelete(s_state_mutex);
        s_state_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }
    BaseType_t created = xTaskCreate(attr_report_task, "matter_attr_rpt", MATTER_ATTR_TASK_STACK, NULL,
                                     MATTER_ATTR_TASK_PRIO, &s_attr_report_task);
    if (created != pdPASS) {
        vQueueDelete(s_attr_report_queue);
        s_attr_report_queue = NULL;
        vSemaphoreDelete(s_state_mutex);
        s_state_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t subscribe_node(uint64_t node_id, const char *rainmaker_node_id)
{
    esp_err_t err = send_wildcard_subscribe(node_id);
    if (err != ESP_OK) {
        return err;
    }
    node_state_t *ns = create_node_state(node_id, rainmaker_node_id);
    if (!ns) {
        return ESP_ERR_NO_MEM;
    }
    ns->next = s_node_states;
    s_node_states = ns;
    return ESP_OK;
}

void matter_attr_report_on_device_list_updated(void)
{
    matter_device_t *dev_list = fetch_device_list();
    if (!dev_list) {
        return;
    }

    uint64_t removed_node_ids[MATTER_ATTR_MAX_REMOVED_PER_UPDATE];
    size_t removed_count = 0;

    {
        xSemaphoreTake(s_state_mutex, portMAX_DELAY);

        /* Remove states for nodes no longer in the list (CHIP shutdown done after mutex is released). */
        node_state_t *n = s_node_states;
        node_state_t *prev = NULL;
        while (n != NULL) {
            node_state_t *next = n->next;
            if (!is_node_in_list(dev_list, n->node_id)) {
                uint64_t node_id = n->node_id;
                if (prev) {
                    prev->next = next;
                } else {
                    s_node_states = next;
                }
                free_node_state(n);
                if (removed_count < MATTER_ATTR_MAX_REMOVED_PER_UPDATE) {
                    removed_node_ids[removed_count++] = node_id;
                } else {
                    ESP_LOGW(TAG, "Removed node backlog > %d; call shutdown manually for 0x%llX",
                             MATTER_ATTR_MAX_REMOVED_PER_UPDATE, (unsigned long long)node_id);
                }
                n = next;
                continue;
            }
            prev = n;
            n = next;
        }

        /* Subscribe to nodes in the list that we don't have yet */
        for (matter_device_t *d = dev_list; d != NULL; d = d->next) {
            if (!find_node_state(d->node_id)) {
                subscribe_node(d->node_id, d->rainmaker_node_id);
            }
        }

        xSemaphoreGive(s_state_mutex);
    }

    /* Must not hold s_state_mutex: shutdown runs subscribe_done/on_subscribe_done_cb which takes the same mutex. */
    for (size_t i = 0; i < removed_count; i++) {
        esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
        send_shutdown_subscriptions(removed_node_ids[i]);
    }

    free_matter_device_list(dev_list);
}
