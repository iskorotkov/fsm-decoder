#pragma once

namespace fsm_decoder
{
    enum class state
    {
        search_sync,
        read_sync,
        read_length,
        read_package,
        read_crc,
        write_header,
        write_package,
        print_stats,
        exiting
    };
}
