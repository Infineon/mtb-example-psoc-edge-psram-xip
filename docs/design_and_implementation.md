[Click here](../README.md) to view the README.

## Design and implementation

The design of this application is minimalistic to get started with code examples on PSOC&trade; Edge MCU devices. All PSOC&trade; Edge E84 MCU applications have a dual-CPU three-project structure to develop code for the CM33 and CM55 cores. The CM33 core has two separate projects for the secure processing environment (SPE) and non-secure processing environment (NSPE). A project folder consists of various subfolders, each denoting a specific aspect of the project. The three project folders are as follows:

**Table 1. Application projects**

Project | Description
--------|------------------------
*proj_cm33_s* | Project for CM33 secure processing environment (SPE)
*proj_cm33_ns* | Project for CM33 non-secure processing environment (NSPE)
*proj_cm55* | CM55 project

<br>

In this code example, at device reset, the secure boot process starts from the ROM boot with the secure enclave (SE) as the root of trust (RoT). From the secure enclave, the boot flow is passed on to the system CPU subsystem where the secure CM33 application starts. After all necessary secure configurations, the flow is passed on to the non-secure CM33 application. Resource initialization for this example is performed by this CM33 non-secure project. It configures the system clocks, pins, clock to peripheral connections, and other platform resources. It then enables the CM55 core using the `Cy_SysEnableCM55()` function and the CM55 core is subsequently put to DeepSleep mode.

In the CM33 non-secure application, the clocks and system resources are initialized by the BSP initialization function. The retarget-io middleware is configured to use the debug UART. The debug UART prints a message (as shown in [Terminal output on program startup](../images/cache-disable-output.png)) on the terminal emulator, the onboard KitProg3 acts the USB-UART bridge to create the virtual COM port. The User LED1 blinks every 1 second.

If cache is enabled, it is configured to make the SMIF-1 region cacheable and with the attributes set to write back, read, and write allocate to demonstrate cache clean and invalidation operations.

In XIP mode, the memory read/write operations can be performed directly without using SMIF APIs. For instance, pointer to the memory address in SMIF memory region can be used directly to read/write data. If cache is enabled, the data is transferred through the cache and follows the cache hit/miss protocols.

In MMIO mode, the target memory attached to the SMIF region works like an external serial memory and would require SMIF library APIs with the address offset to read/write data – all data transfers bypass the cache.

XIP and MMIO modes are used to demonstrate cache operations in this code example in the following ways:

- **SMIF cache disabled:** When SMIF cache is disabled, all the read/write data transfers bypass the SMIF cache, and hence, data reads always match the actual data in target memory

- **SMIF cache enabled:** When SMIF cache is enabled, the memory read/write operations pass through the SMIF cache and follows the cache properties and rules

   - **Cache clean:** When writing data (in XIP mode) to the target memory address where SMIF-1 starts, a cache hit, if occurred, causes the data to be written to the cache first. Since cache is configured with the write back attribute, the data in the cache and target memory (MMIO read) will differ, as shown in the terminal output. When the cache clean operation is performed, the data is transferred from the cache to the target memory.

   - **Cache invalidate:** Data is written to the target memory address in MMIO mode, upon reading the same address using XIP mode, the data is different from what was written. when performing cache invalidation, the data from the target memory now moves to the cache, the same is shown in the terminal output.

<br>