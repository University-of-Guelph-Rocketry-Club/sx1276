# Remaining Features
* Ensure payload and the payload header (everything related to the payload basically) is **packed** and offsets are "strict".
    * Can also implement a decoder function with precomputed offsets based on padding, but being able to just cast to a struct is nice.
* Determine recipient ID to use for each device (ground and satellite/rocket "station")
    * Similarily, determine what the GSC_IDENTITY value should be. This has to be hardcoded in the header and cannot be `0xFF`!
* Determine what data to send and how to interpret it based on satellite commands
* Implement CRC hash validation on RX.
