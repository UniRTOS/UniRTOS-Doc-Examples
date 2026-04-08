#include "qosa_def.h"
#include "qosa_nvitem.h"
#include "qosa_log.h"

#define QOS_LOG_TAG                           LOG_TAG

/* =========================================================================
 * 2. Application Layer: Define business structures and global variables
 * ========================================================================= */

// Business configuration structure (Decoupled, members can be adjusted freely)
typedef struct
{
    int boot_count;       // Boot count
    char device_name[32]; // Device name string
    int auto_sleep_en;    // Auto-sleep enable (1: enable, 0: disable)
} sys_config_t;

// Declare a global configuration variable (memory data carrier)
sys_config_t g_sys_cfg;

// Define the save path and node name for this group of configs in the file system
// Format: /filename/root_node/sub_node_name
#define SYS_CFG_NV_PATH "application_cfg.json/config/example_cfg"

/* =========================================================================
 * 3. Core Business Logic: Read, Write, and Delete test flow
 * ========================================================================= */

void json_nv_demo_task(void)
{
    qosa_nvm_error_e ret;

    // ---------------------------------------------------------
    // Step 1: Build the "Parameter Mapping Table" (Binding Table)
    // Core idea: Give each member in the structure a JSON name
    //            and bind its memory address.
    // ---------------------------------------------------------
    qosa_nv_cfg_item_data_t sys_cfg_table[] = {
        //  JSON Key name        Variable memory address          Variable byte size
        {"boot_count", &g_sys_cfg.boot_count, sizeof(g_sys_cfg.boot_count)},
        {"device_name", g_sys_cfg.device_name, sizeof(g_sys_cfg.device_name)}, // The array name itself is an address, no '&' needed

        NV_ITEM_CFG_DEF("auto_sleep_en", g_sys_cfg.auto_sleep_en)};

    // Calculate the number of parameters in the mapping table
    qosa_uint16_t item_count = sizeof(sys_cfg_table) / sizeof(sys_cfg_table[0]);

    // ---------------------------------------------------------
    // Step 2: Write parameters (Write)
    // ---------------------------------------------------------
    QLOGI("\n--- [1] Executing configuration write ---\n");
    // Simulate the application layer modifying the parameters in memory
    g_sys_cfg.boot_count = 100;
    qosa_strcpy(g_sys_cfg.device_name, "IoT_Node");
    g_sys_cfg.auto_sleep_en = 1;

    ret = qosa_nv_item_json_write(SYS_CFG_NV_PATH, sys_cfg_table, item_count);
    if (ret == QOSA_NVM_CFG_OK)
    {
        QLOGI("[SUCCESS] Parameters written to the file system successfully! (Redundancy check mechanism triggered)\n");
    }
    else
    {
        QLOGI("[ERROR] Write failed, error code: %d\n", ret);
    }

    // ---------------------------------------------------------
    // Step 3: Read parameters (Read)
    // ---------------------------------------------------------
    QLOGI("\n--- [2] Executing configuration read ---\n");
    // To prove the data is actually read from the file, we intentionally clear the memory first
    qosa_memset(&g_sys_cfg, 0, sizeof(sys_config_t));
    QLOGI("Memory values after clearing -> Boot count: %d, Device name: %s\n", g_sys_cfg.boot_count, g_sys_cfg.device_name);

    ret = qosa_nv_item_cfg_read(SYS_CFG_NV_PATH, sys_cfg_table, item_count);
    if (ret == QOSA_NVM_CFG_OK)
    {
        QLOGI("[SUCCESS] Parameters read successfully!\n");
        // Print and verify the recovered values
        QLOGI("Recovered memory values -> Boot count: %d, Device name: %s, Sleep enable: %d\n",
               g_sys_cfg.boot_count,
               g_sys_cfg.device_name,
               g_sys_cfg.auto_sleep_en);
    }
    else
    {
        QLOGI("[ERROR] Read failed, error code: %d\n", ret);
    }

    // ---------------------------------------------------------
    // Step 4: Delete parameters (Delete)
    // ---------------------------------------------------------
    QLOGI("\n--- [3] Executing configuration delete ---\n");
    // Assume a factory reset is needed, so we delete the 'sys_config' node
    ret = qosa_nv_item_cfg_delete(SYS_CFG_NV_PATH, sys_cfg_table, item_count);
    if (ret == QOSA_NVM_CFG_OK)
    {
        QLOGI("[SUCCESS] Parameter node deleted successfully! The JSON file no longer contains 'sys_config'.\n");
    }
    else
    {
        QLOGI("[ERROR] Delete failed, error code: %d\n", ret);
    }
}

int unir_json_nv_demo_init(void)
{
    QLOGI("========== JSON NV Solution Test Demo Started ==========\n");
    json_nv_demo_task();
    QLOGI("========== JSON NV Solution Test Demo Finished ==========\n");
    return 0;
}