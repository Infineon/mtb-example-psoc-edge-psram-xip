/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for CM33 Secure Project.
*
* Related Document: See README.md
*
*
*******************************************************************************
* (c) 2023-2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/

#include "cy_pdl.h"
#include "cybsp.h"
#include "retarget_io_init.h"
#include "cycfg_qspi_memslot.h"
#include "mtb_serial_memory.h"


/*****************************************************************************
* Macros
******************************************************************************/
#define CM33_NS_APP_BOOT_ADDR      (CYMEM_CM33_0_m33_nvm_START + \
                                       CYBSP_MCUBOOT_HEADER_SIZE)
#define CACHE_ENABLE                    (0U)
#define XIP_ENCRYPT_ENABLE              (0U)
#define ADDRESS_SIZE_IN_BYTES           (4U)
#define NUM_BYTES_PER_LINE              (16U)
#define SMIF_MMIO_ADDRESS_OFFSET        (0U)
#define SMIF_1_PSRAM_SECURE_ADDRESS     (0x74000000U)
#define TEST_DATA_1                     (0xAA)
#define TEST_DATA_2                     (0x55)
#define SMIF_INIT_TIMEOUT_USEC          (10000U)
#define DEFAULT_DATA_VAL                (0U)
#define CACHE_OPERATION_WAIT_MS         (10U)

#if (1U == XIP_ENCRYPT_ENABLE)
#define CRYPTO_KEY_SIZE                 (16u)     /* Crypto key size in bytes - Do not edit */
#define INIT_VECTOR_SIZE                (12u)     /* Crypto Initialization vector */
#define XIP_ADDRESS                     0x74000000UL
cy_stc_smif_context_t smif_context;
#endif

/******************************************************************************
 * Global Variables
 ******************************************************************************/
static mtb_serial_memory_t serial_memory_obj;
static cy_stc_smif_mem_context_t smif_mem_context;
static cy_stc_smif_mem_info_t smif_mem_info;

/*******************************************************************************
 * Function Name: memory_compare
 ********************************************************************************
 * Summary:
 *
 *  Compare two memory regions.
 *
 * Parameters:
 *
 *  dst:  Pointer to the destination memory region.
 *  test_data:  Pointer to the source memory region.
 *  size: Size of the memory regions to compare.
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void memory_compare(void *dst, void *test_data, const uint32_t size)
{
    if (memcmp(dst, test_data, size) == 0)
    {
        printf("Memory compare successful: Data matches test data\r\n");
    }
    else
    {
        printf("Memory compare failed: Data mismatch. Expected data and "
                "read data are different\r\n");
    }
}

/*******************************************************************************
 * Function Name: check_status
 *******************************************************************************
 * Summary:
 *
 *  Prints the message, indicates the non-zero status by turning the LED on, and
 *  asserts the non-zero status.
 *
 * Parameters:
 *
 *  message:    Message to print if status is non-zero.
 *  status:     Status for evaluation.
 *
 * Return:
 *  void
 *
 ******************************************************************************/
static void check_status(char *message, uint32_t status)
{
    if (status)
    {
        printf("\n\r====================================================\n\r");
        printf("\n\rFAIL: %s\n\r", message);
        printf("Error Code: 0x%x\n\r", (int) status);
        printf("\n\r====================================================\n\r");
        handle_error();
    }
}

/*******************************************************************************
 * Function Name: print_array
 *******************************************************************************
 * Summary:
 *  Prints the content of the buffer to the UART console.
 *
 * Parameters:
 *
 *  message:        Message to print before array output.
 *  buf:            Buffer to print on the console.
 *  size:           Size of the buffer.
 *
 * Return:
 *  void
 *
 ******************************************************************************/
static void print_array(char *message, uint8_t *buf, const uint32_t size)
{
    printf("\r\n%s (%u bytes):\r\n", message, (unsigned int)size);
    printf("-------------------------\r\n");

    for (uint32_t index = 0; index < size; index++)
    {
        printf("0x%02X ", buf[index]);

        /* To print 8 data values per line, switch to the next line
         * after 8 values have been printed. */
        if (((index + 1) % 8) == 0)
        {
            printf("\r\n");
        }

        /* If the maximum number of bytes per line is reached, start
         *  a new line. */
        if (0u == ((index + 1) % NUM_BYTES_PER_LINE))
        {
            printf("\r\n");
        }
    }
}

#if (1U == CACHE_ENABLE) && (0U == XIP_ENCRYPT_ENABLE)
/******************************************************************************
 * Function Name: memory_cache_clean_demo
 *******************************************************************************
 * Summary:
 *  This function demonstrates the process of writing to a PSRAM memory,
 *  performing cache clean operation, and comparing the data before and after
 *  the cache clean operation.
 *
 * Parameters:
 *
 *  psram_start :  The start address of the PSRAM memory.
 *  byte_pattern:  The test data to be written to the PSRAM memory.
 *  size        :  Size of the buffer.
 *
 * Return:
 *  void
 *
 ******************************************************************************/

static void memory_cache_clean_demo(uint32_t psram_start, uint8_t byte_pattern, const uint32_t size)
{
    /** Define test data and memory buffer */
    uint8_t *psram = (uint8_t *)psram_start;
    uint8_t test_data[NUM_BYTES_PER_LINE];
    uint8_t serial_buffer[NUM_BYTES_PER_LINE];

    printf("\r\n\n******** Memory cache clean demo ********\r\n");

    /** Fill test_data array with test_data */
    memset(test_data, byte_pattern, size);

    /** Clear the PSRAM memory */
    memset(psram, DEFAULT_DATA_VAL, size);

    print_array("Test data", (uint8_t *)test_data, size);

    /** Perform memory write using memcpy */
    memcpy(psram, test_data, size);

    mtb_serial_memory_read(&serial_memory_obj, SMIF_MMIO_ADDRESS_OFFSET, size, serial_buffer);

    print_array("Data read before cache clean", (uint8_t *)serial_buffer, size);

    /** Compare serial_buffer and test data */
    memory_compare(serial_buffer, test_data, size);

    printf("Perform cache clean operation\r\n");

    Cy_SMIF_Clean_All_Cache(SMIF1_CACHE_BLOCK);
    Cy_SysLib_Delay(CACHE_OPERATION_WAIT_MS);

    mtb_serial_memory_read(&serial_memory_obj, SMIF_MMIO_ADDRESS_OFFSET, size, serial_buffer);

    print_array("Data read after cache clean", (uint8_t *)serial_buffer, size);

    /** Compare serial_buffer and test data */
    memory_compare(serial_buffer, test_data, size);

}

/******************************************************************************
 * Function Name: memory_cache_invalidate_demo
 *******************************************************************************
 * Summary:
 * This function demonstrates the process of writing to a PSRAM memory,
 * performing cache invalidate operation, and comparing the data before and after
 * the cache invalidate operation.
 *
 * Parameters:
 *
 *  psram_start :  The start address of the PSRAM memory.
 *  byte_pattern:  The test data to be written to the PSRAM memory.
 *  size        :  Size of the buffer.
 *
 * Return:
 *  void
 *
 ******************************************************************************/
static void memory_cache_invalidate_demo(uint32_t psram_start, uint8_t byte_pattern, const uint32_t size)
{
    /** Define test data and memory buffer */
    uint8_t *psram = (uint8_t *)psram_start;
    uint8_t test_data[NUM_BYTES_PER_LINE];
    uint8_t xip_buffer[NUM_BYTES_PER_LINE];

    printf("\r\n\n******** Memory cache invalidate demo ********\r\n");

    /** Fill test_data array with test_data */
    memset(test_data, byte_pattern, size);

    /** Clear the PSRAM memory */
    memset(psram, DEFAULT_DATA_VAL, size);

    print_array("Test data", (uint8_t *)test_data, size);

    /** Perform memory write using serial memory write API */
    mtb_serial_memory_write(&serial_memory_obj, SMIF_MMIO_ADDRESS_OFFSET, size, test_data);

    /** Read data from PSRAM into xip_buffer */
    memcpy(xip_buffer, psram, size);

    print_array("Data read before cache invalidate", (uint8_t *)xip_buffer, size);

    /** Compare the data in PSRAM with the data in xip_buffer */
    memory_compare(xip_buffer, test_data, size);

    printf("Perform cache invalidate\r\n");

    /** Invalidate the cache for PSRAM */
    Cy_SMIF_Invalidate_All_Cache(SMIF1_CACHE_BLOCK);

    /** Delay for cache invalidate operation to complete */
    Cy_SysLib_Delay(CACHE_OPERATION_WAIT_MS);

    /** Read data from PSRAM into xip_buffer */
    memcpy(xip_buffer, psram, size);

    print_array("Data read after cache invalidate", (uint8_t *)xip_buffer, size);

    /** Compare the data in PSRAM with the data in xip_buffer */
    memory_compare(xip_buffer, test_data, size);

}

#elif (0U == CACHE_ENABLE)
/******************************************************************************
* Function Name: memory_read_write_demo
*******************************************************************************
* Summary:
*  This function demonstrates the process of writing to a PSRAM memory, reading
*  back the data, and comparing it with the original data
*
* Parameters:
*  psram_start   : The start address of the PSRAM memory
*  byte_pattern  : The test data to be written to the PSRAM memory.
*  size          : The size of the memory region to be tested.
*
* Return:
*  void
*
******************************************************************************/

static void memory_read_write_demo(uint32_t psram_start, uint8_t byte_pattern, const uint32_t size)
{
    /** Define test data and memory buffer */
    uint8_t *psram = (uint8_t *)psram_start;
    uint8_t test_data[NUM_BYTES_PER_LINE];
    uint8_t serial_buffer[NUM_BYTES_PER_LINE];

#if (1U == XIP_ENCRYPT_ENABLE)
    cy_rslt_t result;
    cy_en_smif_status_t smif_status = CY_SMIF_BAD_PARAM;
    bool smif_mode, crypto_status = false;
    uint8_t encrypted_test_data[NUM_BYTES_PER_LINE];

    /* The AES key. Writing of the AES key should be from secure area of the code */
    uint8_t cryptoKey[CRYPTO_KEY_SIZE] = {0x54U, 0x68U, 0x61U, 0x74U, 0x73U, 0x20U, 0x6DU, 0x79U,
                                         0x20U, 0x4BU, 0x75U, 0x6EU, 0x67U, 0x20U, 0x46U, 0x75U};
    uint8_t cryptoIV[INIT_VECTOR_SIZE] = {0x1FU, 0x34U, 0x65U, 0x94U, 0x73U, 0x24U, 0x1DU, 0x89U,
                                         0x25U, 0x4AU, 0x77U, 0x4DU};
#endif

    printf("\r\n\n******** Memory read write demo ********\r\n");

    /** Fill test_data array with test_data */
    memset(test_data, byte_pattern, size);

    print_array("Test data", (uint8_t*)test_data, size);

#if (0U == XIP_ENCRYPT_ENABLE)
    /** Clear the PSRAM memory */
    memset(psram, DEFAULT_DATA_VAL, size);

    /** Perform memory write using memcpy */
    memcpy(psram, test_data, size);

    mtb_serial_memory_read(&serial_memory_obj, SMIF_MMIO_ADDRESS_OFFSET, size, serial_buffer);
    print_array("Destination data after memory copy", (uint8_t*)psram, size);
#endif

#if (1U == XIP_ENCRYPT_ENABLE)
    /* Fill the encryptedtxBuffer with sample data to be transmitted */
    for(uint32_t index = 0; index < NUM_BYTES_PER_LINE; index++)
    {
        encrypted_test_data[index] = test_data[index];
    }

    /* Set the crypto key */
    Cy_SMIF_SetCryptoKey(SMIF1_CORE, (uint32_t *)cryptoKey);

    /* Set Initialization Vector */
    Cy_SMIF_SetCryptoIV(SMIF1_CORE, (uint32_t *)cryptoIV);

    /* Encrypt data in encrypted_test_data */
    smif_status = Cy_SMIF_Encrypt(SMIF1_CORE, XIP_ADDRESS, encrypted_test_data, NUM_BYTES_PER_LINE, &smif_context);
    check_status("[Error] Data encryption failed\r\n\r\n", smif_status);

#if (CY_IP_MXSMIF_VERSION>=5)
    cy_stc_smif_crypto_region_config_t region_config;
    region_config.key[0] = ((uint32_t *)cryptoKey)[0];
    region_config.key[1] = ((uint32_t *)cryptoKey)[1];
    region_config.key[2] = ((uint32_t *)cryptoKey)[2];
    region_config.key[3] = ((uint32_t *)cryptoKey)[3];

    region_config.iv[0] = 0; //nonce
    region_config.iv[1] = ((uint32_t *)cryptoIV)[0];
    region_config.iv[2] = ((uint32_t *)cryptoIV)[1];
    region_config.iv[3] = ((uint32_t *)cryptoIV)[2];

    /* region_base_address must be the XIP mapped address (S-world alias) that the CPU fetches from.
     * Using 0x0000 would mean the region does not cover any valid XIP address. */
    region_config.region_base_address = (uint32_t *)XIP_ADDRESS;
    region_config.region_size = 1024;

    /* Configures a crypto region with specified start address, size and keys */
    Cy_SMIF_SetCryptoKeyRegion(SMIF1_CORE, 0U, &region_config);

    /* Enable On-the-fly hardware decryption for this slave */
    smif_status = Cy_SMIF_SetCryptoEnable(SMIF1_CORE, smif1MemConfigs[0]->slaveSelect);
    check_status("Cy_SMIF_SetCryptoEnable failed \r\n", smif_status);
#endif

    /* Confirm hardware crypto is enabled before switching to XIP mode */
    (void)Cy_SMIF_IsCryptoEnabled(SMIF1_CORE, smif1MemConfigs[0]->slaveSelect, &crypto_status);
    printf("On-the-fly crypto enabled (before XIP mode): %d\r\n", (int)crypto_status);
    if (!crypto_status)
    {
        printf("Crypto not enabled \r\n");
    }

    smif_mode = Cy_SMIF_GetMode(SMIF1_CORE);
    if(smif_mode == CY_SMIF_NORMAL)
    {
        printf("Switching to XIP mode...\r\n");
        Cy_SMIF_SetMode(SMIF1_CORE, CY_SMIF_MEMORY);
    }
    else
    {
        handle_error();
    }

    /** Clear the PSRAM memory */
    memset(psram, DEFAULT_DATA_VAL, size);
    print_array("Cleared the memory before writing encrypted data", (uint8_t*)psram, size);

    print_array("Encrypted data", (uint8_t*)encrypted_test_data, NUM_BYTES_PER_LINE);

    /* Writing encrypted data to memory */
    result = mtb_serial_memory_write(&serial_memory_obj, SMIF_MMIO_ADDRESS_OFFSET, NUM_BYTES_PER_LINE, encrypted_test_data);
    check_status("serial memory write failed", result);
    printf("Writing encrypted data to memory - SUCCESS \r\n");

    memcpy(serial_buffer, (const uint8_t *)XIP_ADDRESS, NUM_BYTES_PER_LINE);
    print_array("On-the-fly (OTF) Decrypted data", serial_buffer, NUM_BYTES_PER_LINE);
#endif

    /** Compare the data in PSRAM with the data in serial_buffer */
    memory_compare(serial_buffer, test_data, size);
}

#endif

/*******************************************************************************
* Function Name: smif_ospi_psram_init
********************************************************************************
* Summary:
*  Initialize SMIF for PSRAM (QSPI Core 1) and set-up serial memory interfeace
*
* Parameters:
*  none
*
* Return:
*  void
*
*******************************************************************************/
static void smif_ospi_psram_init(void)
{
    cy_rslt_t result;

    /* Disable SMIF Block for reconfiguration. */
    Cy_SMIF_Disable(CYBSP_SMIF_CORE_1_PSRAM_HW);

    /* Initialize SMIF-1 Peripheral. */
    result = Cy_SMIF_Init((CYBSP_SMIF_CORE_1_PSRAM_hal_config.base),
                           (CYBSP_SMIF_CORE_1_PSRAM_hal_config.config),
                           SMIF_INIT_TIMEOUT_USEC, &smif_mem_context.smif_context);

    check_status("Cy_SMIF_Init failed", result);

    /* Configure Data Select Option for SMIF-1 */
    Cy_SMIF_SetDataSelect(CYBSP_SMIF_CORE_1_PSRAM_hal_config.base,
                          smif1BlockConfig.memConfig[0]->slaveSelect,
                          smif1BlockConfig.memConfig[0]->dataSelect);

    /* Enable the SMIF_CORE_1 block. */
    Cy_SMIF_Enable(CYBSP_SMIF_CORE_1_PSRAM_hal_config.base, &smif_mem_context.smif_context);

    /* Set-up serial memory. */
    result = mtb_serial_memory_setup(&serial_memory_obj,
                                MTB_SERIAL_MEMORY_CHIP_SELECT_2,
                                CYBSP_SMIF_CORE_1_PSRAM_hal_config.base,
                                CYBSP_SMIF_CORE_1_PSRAM_hal_config.clock,
                                &smif_mem_context,
                                &smif_mem_info,
                                &smif1BlockConfig);

    check_status("serial memory setup failed", result);
}

/*****************************************************************************
* Function Name: main
******************************************************************************
* This is the main function for Cortex M33 CPU secure application
* NOTE: CM33 secure project assumes that certain memory and peripheral regions
* will be accessed from non-secure environment by the CM33 NS /CM55 code,
* For such regions MPC and PPC configurations are applied by cybsp_init to make
* it non-secure. Any access to these regions from the secure side is recommended
* to be done before the MPC/PPC configuration is applied.Once a memory or
* peripheral region is marked as non secure it cannot be accessed from the secure
* side using secure aliased address but may be accessed using non secure aliased
* address

* NOTE: In this code example we skip the MPC and PPC initializations in
* cybsp_init using following Makefile defines:
* DEFINES+=CYBSP_SKIP_MPC_INIT
* DEFINES+=CYBSP_SKIP_PPC_INIT
* This allows initialization of SMIF clock and peripheral from secure project
*****************************************************************************/
int main(void)
{
    uint32_t ns_stack;
    cy_cmse_funcptr NonSecure_ResetHandler;
    cy_rslt_t result;

    /* Set up internal routing, pins, and clock-to-peripheral connections */
    result = cybsp_init();

    /* Board initialization failed. Stop program execution */
    if (CY_RSLT_SUCCESS != result)
    {
        handle_error();
    }

    /* Enable global interrupts */
    __enable_irq();

    /*
    * Initialize the clock for the APP_MMIO_TCM (512K) peripheral group.
    * This sets up the necessary clock and peripheral routing to ensure
    * the APP_MMIO_TCM can be correctly accessed and utilized.
    */
    Cy_SysClk_PeriGroupSlaveInit(
        CY_MMIO_CM55_TCM_512K_PERI_NR,
        CY_MMIO_CM55_TCM_512K_GROUP_NR,
        CY_MMIO_CM55_TCM_512K_SLAVE_NR,
        CY_MMIO_CM55_TCM_512K_CLK_HF_NR
    );

    /*
    * Initialize the clock for the SMIF0 peripheral group.
    * This sets up the necessary clock and peripheral routing to ensure
    * the SMIF0 can be correctly accessed and utilized.
    */
    Cy_SysClk_PeriGroupSlaveInit(
        CY_MMIO_SMIF0_PERI_NR,
        CY_MMIO_SMIF0_GROUP_NR,
        CY_MMIO_SMIF0_SLAVE_NR,
        CY_MMIO_SMIF0_CLK_HF_NR
    );

    /* Initialize SMIF in QSPI mode */
    if (CY_RSLT_SUCCESS != result)
    {
        handle_error();
    }

    /* Memory protection initialization */
    result = Cy_MPC_Init();
    if (CY_RSLT_SUCCESS != result)
    {
        handle_error();
    }

    init_retarget_io();

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H"); 

    printf("****************** "
            "PSOC Edge MCU: PSRAM Read and Write in eXecute-In-Place (XIP) mode"
            "****************** \r\n\n");

#if (1U == CACHE_ENABLE)
    /* Cache attributes set to Write Back, Read & Write Allocate, to demonstrate
     * cache clean and invalidation operations. */
    cy_stc_smif_cache_region_t cache_region_0 =
    {
        .enabled = true,
        .start_address = SMIF_1_PSRAM_SECURE_ADDRESS,
        .end_address = SMIF_1_PSRAM_SECURE_ADDRESS + CY_XIP_PORT1_SIZE,
        .cache_attributes = CY_SMIF_CACHEABLE_WB_RWA
    };

    cy_stc_smif_cache_config_t cache_config =
    {
        .enabled = true,
        .cache_retention_on = true,
    };

    memcpy(&cache_config .cache_region_0, &cache_region_0, sizeof(cache_region_0));

    Cy_SMIF_InitCache(SMIF1_CACHE_BLOCK, &cache_config);

    printf("Cache is Enabled\r\n");
#else
    printf("Cache is Disabled\r\n");
#endif

    /* Initialize PSRAM and set-up serial memory */
    smif_ospi_psram_init();

    check_status("smif_ospi_psram_init error", (uint32_t)result);

#if (0U == XIP_ENCRYPT_ENABLE)
    printf("XIP Encryption is Disabled\r\n");
    /* Enable XIP mode for the SMIF memory slot associated with the PSRAM. */
    result = mtb_serial_memory_enable_xip(&serial_memory_obj, true);
    check_status("mtb_serial_memory_enable_xip: failed", result);
#else
    printf("XIP Encryption is Enabled\r\n");
#endif

    /* Enable write for the SMIF memory slot associated with the PSRAM. */
    result = mtb_serial_memory_set_write_enable(&serial_memory_obj, true);
    check_status("mtb_serial_memory_set_write_enable: failed", result);

#if (1U == CACHE_ENABLE) && (0U == XIP_ENCRYPT_ENABLE)
    memory_cache_clean_demo(SMIF_1_PSRAM_SECURE_ADDRESS, TEST_DATA_2, NUM_BYTES_PER_LINE);
    memory_cache_invalidate_demo(SMIF_1_PSRAM_SECURE_ADDRESS, TEST_DATA_1, NUM_BYTES_PER_LINE);
#elif (1U == CACHE_ENABLE) && (1U == XIP_ENCRYPT_ENABLE)
    printf("\r\nWhen XIP_ENCRYPT_ENABLE is set to 1U, the CACHE_ENABLE macro should be set to 0U (the cache-disabled path), as the encryption demo operates in the non-cache code path.\r\n");
#elif (0U == CACHE_ENABLE)
    memory_read_write_demo(SMIF_1_PSRAM_SECURE_ADDRESS, TEST_DATA_1, NUM_BYTES_PER_LINE);
#endif

    while (cy_retarget_io_is_tx_active());

    /* Peripheral protection initialization (PPC0) */
    result = Cy_PPC0_Init();
    if (CY_RSLT_SUCCESS != result)
    {
        handle_error();
    }

    /* Peripheral protection initialization (PPC1) */
    result = Cy_PPC1_Init();
    if (CY_RSLT_SUCCESS != result)
    {
        handle_error();
    }

    ns_stack = (uint32_t)(*((uint32_t*)CM33_NS_APP_BOOT_ADDR));
    __TZ_set_MSP_NS(ns_stack);

    NonSecure_ResetHandler = (cy_cmse_funcptr)(*((uint32_t*)(CM33_NS_APP_BOOT_ADDR + 4)));

    /* Start non-secure application */
    NonSecure_ResetHandler();

    for (;;)
    {
    }
}

/* [] END OF FILE */
