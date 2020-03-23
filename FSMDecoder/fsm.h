#pragma once
#include <iostream>
#include <fstream>
#include <optional>
#include <vector>

#include "state.h"
#include "crc.h"

namespace fsm_decoder
{
    using namespace std::string_literals;

    const auto input_filename = "data/data_no_error.bin"s;
    const auto output_filename = "data/output.txt"s;

    const auto expected_sync = std::byte(0xAA);
    const auto expected_id = std::byte(0x87);
    const auto expected_length = std::byte(45);

    state current_state = state::search_sync;

    auto packages_total = 0;
    auto packages_valid = 0;
    std::ifstream input;
    std::ofstream output;
    std::vector<std::byte> data;

    std::optional<std::byte> next_byte()
    {
        if (input)
        {
            return std::byte(input.get());
        }
        return {};
    }

    std::optional<std::vector<std::byte>> next_bytes(
            const int length = std::to_integer<int>(expected_length) - 2
        )
    {
        const auto bytes_count = input.tellg();

        auto buffer = new std::byte[length];
        input.read((char*)buffer, length);
        std::vector vec(buffer, buffer + input.tellg() - bytes_count);

        if (vec.size() == length)
        {
            return vec;
        }
        return {};
    }

    void search_sync()
    {
        auto b = next_byte();
        if (b.has_value())
        {
            if (b.value() == expected_sync)
            {
                current_state = state::read_sync;
            }
            else
            {
                current_state = state::search_sync;
            }
        }
        else
        {
            current_state = state::print_stats;
        }  
    }

    void read_sync()
    {
        auto b = next_byte();
        if (b.has_value())
        {
            if (b.value() == expected_sync)
            {
                current_state = state::read_length;
            }
            else
            {
                current_state = state::search_sync;
            }
        }
        else
        {
            current_state = state::print_stats;
        }
    }

    void read_length()
    {
        auto b = next_byte();
        if (b.has_value())
        {
            if (b.value() == expected_length)
            {
                current_state = state::read_package;
            }
            else
            {
                current_state = state::search_sync;
            }
        }
        else
        {
            current_state = state::print_stats;
        }
    }

    void read_package()
    {
        auto bytes = next_bytes();
        if (bytes.has_value())
        {
            data = std::move(bytes.value());

            if (data[0] == expected_id)
            {
                current_state = state::read_crc;
            }
            else
            {
                current_state = state::search_sync;
            }
        }
        else
        {
            current_state = state::print_stats;
        }
    }

    void read_crc()
    {
        auto b1 = next_byte();
        auto b2 = next_byte();
        if (b1.has_value() && b2.has_value())
        {
            auto v1 = std::to_integer<uint16_t>(b1.value());
            auto v2 = std::to_integer<uint16_t>(b2.value());
            auto expected_crc = (v1 << 8) | v2;
            auto actual_crc = crc16((uint8_t*)data.data(), data.size());
            if (actual_crc == expected_crc)
            {
                current_state = packages_valid > 0
                    ? state::write_package
                    : state::write_header;
                ++packages_valid;
            }
            else
            {
                current_state = state::search_sync;
            }
            ++packages_total;
        }
        else
        {
            current_state = state::print_stats;
        }
    }

    void write_header()
    {
        output.open(output_filename);
        output << "Ax" << "Ay" << "Az" << "Wx" << "Wy" << "Wz"
            << "Tax" << "Tay" << "Taz" << "Twx" << "Twy" << "Twz"
            << "S" << "Timestamp" << "Status" << "Number" << "\n";
        current_state = state::write_package;
    }

    void write_package()
    {
        // TODO: write package data
        output << "new package\n";
        current_state = state::search_sync;
    }

    void print_stats()
    {
        std::cout << "Packages\n"
            << "total: " << packages_total << ", valid: " << packages_valid << "\n";
        current_state = state::exiting;
    }

    void step()
    {
        switch (current_state)
        {
        case state::search_sync:
            search_sync();
            break;
        case state::read_sync:
            read_sync();
            break;
        case state::read_length:
            read_length();
            break;
        case state::read_package:
            read_package();
            break;
        case state::read_crc:
            read_crc();
            break;
        case state::write_header:
            write_header();
            break;
        case state::write_package:
            write_package();
            break;
        case state::print_stats:
            print_stats();
            break;
        default:
            throw std::exception("State was not implemented.");
            break;
        }
    }

    void run()
    {
        input.open(input_filename, std::ios::binary);
        while (current_state != state::exiting)
        {
            step();
        }
    }
}
