# Remaining Features
* Determine recipient ID to use for each device (ground and satellite/rocket "station"); see `sx1276_transmit_packet` in `chip_main.c`.
    * Similarily, determine what the GSC_IDENTITY value should be. This has to be hardcoded in the header and cannot be `0xFF`! We can define it at build time if we want (e.g. `-DGSC_IDENTITY=0xAB`).
* Determine what data to send and how to interpret it based on satellite commands.
