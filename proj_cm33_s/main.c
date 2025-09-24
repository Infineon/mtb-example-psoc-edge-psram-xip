/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for CM33 Secure Project.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2023-2025, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
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
#define ADDRESS_SIZE_IN_BYTES           (4U)
#define NUM_BYTES_PER_LINE              (16U)
#define SMIF_MMIO_ADDRESS_OFFSET        (0U)
#define SMIF_1_PSRAM_SECURE_ADDRESS     (0x74000000U)
#define TEST_DATA_1                     (0xAA)
#define TEST_DATA_2                     (0x55)
#define SMIF_INIT_TIMEOUT_USEC          (10000U)
#define DEFAULT_DATA_VAL                (0U)
#define CACHE_OPERATION_WAIT_MS         (10U)

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

#if (1U == CACHE_ENABLE)
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

#else
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

    printf("\r\n\n******** Memory read write demo ********\r\n");

    /** Fill test_data array with test_data */
    memset(test_data, byte_pattern, size);

    /** Clear the PSRAM memory */
    memset(psram, DEFAULT_DATA_VAL, size);

    print_array("Test data", (uint8_t*)test_data, size);

    /** Perform memory write using memcpy */
    memcpy(psram, test_data, size);

    mtb_serial_memory_read(&serial_memory_obj, SMIF_MMIO_ADDRESS_OFFSET, size, serial_buffer);

    print_array("Destination data after memory copy", (uint8_t*)psram, size);

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

    /* Enable XIP mode for the SMIF memory slot associated with the PSRAM. */
    result = mtb_serial_memory_enable_xip(&serial_memory_obj, true);
    check_status("mtb_serial_memory_enable_xip: failed", result);

    /* Enable write for the SMIF memory slot associated with the PSRAM. */
    result = mtb_serial_memory_set_write_enable(&serial_memory_obj, true);
    check_status("mtb_serial_memory_set_write_enable: failed", result);

#if (1U == CACHE_ENABLE)
    memory_cache_clean_demo(SMIF_1_PSRAM_SECURE_ADDRESS, TEST_DATA_2, NUM_BYTES_PER_LINE);
    memory_cache_invalidate_demo(SMIF_1_PSRAM_SECURE_ADDRESS, TEST_DATA_1, NUM_BYTES_PER_LINE);
#else
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
