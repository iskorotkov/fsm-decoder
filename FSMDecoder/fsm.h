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

    const auto input_filename = "input.bin"s;
    const auto output_filename = "output.txt"s;

    const auto expected_sync = std::byte('0xAA');
    const auto expected_id = std::byte('0x87');
    const auto expected_length = std::byte(45);

    state current_state = state::search_sync;
    uint16_t crc;
    auto bytes_read = 0;
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
            const int length = std::to_integer<int>(expected_length) - 3
        )
    {
        const auto bytes_count = input.tellg();

        auto buffer = new std::byte[length];
        input.get((char*)buffer, length);
        std::vector vec(buffer, buffer + input.tellg() - bytes_count);

        if (vec.size() == length)
        {
            return vec;
        }
        return {};
    }

    void on_file_ended()
    {
        if (packages_valid > 0)
        {
            current_state = state::write_package;
        }
        else
        {
            current_state = state::write_header;
        }
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
            on_file_ended();
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
            on_file_ended();
        }
    }

    void read_length()
    {
        auto b = next_byte();
        if (b.has_value())
        {
            if (b.value() == expected_length)
            {
                current_state = state::read_data;
            }
            else
            {
                current_state = state::search_sync;
            }
        }
        else
        {
            on_file_ended();
        }
    }

    void read_id()
    {
        auto b = next_byte();
        if (b.has_value())
        {
            if (b.value() == expected_id)
            {
                current_state = state::read_data;
            }
            else
            {
                current_state = state::search_sync;
            }
        }
        else
        {
            on_file_ended();
        }
    }

    void read_data()
    {
        auto bytes = next_bytes();
        if (bytes.has_value())
        {
            data = std::move(bytes.value());
            crc = crc16(data.data(), data.size());
            current_state = state::read_crc;
        }
        else
        {
            on_file_ended();
        }
    }

    void read_crc()
    {
        auto b1 = next_byte();
        auto b2 = next_byte();
        if (b1.has_value() && b2.has_value())
        {
            ++packages_total;

            auto v1 = std::to_integer<uint16_t>(b1.value());
            auto v2 = std::to_integer<uint16_t>(b2.value());
            auto expected_crc = (v1 << 8) | v2;
            if (crc == expected_crc)
            {
                ++packages_valid;
                on_file_ended();
            }
            else
            {
                current_state = state::search_sync;
            }
        }
        else
        {
            on_file_ended();
        }
    }

    void write_header()
    {
        output.open(output_filename);
        output << "Ax" << "Ay" << "Az" << "Wx" << "Wy" << "Wz"
            << "Tax" << "Tay" << "Taz" << "Twx" << "Twy" << "Twz"
            << "S" << "Timestamp" << "Status" << "Number";
        current_state = state::write_package;
    }

    void write_package()
    {
        // TODO: write package data
        current_state = state::search_sync;
    }

    void print_stats()
    {
        std::cout << "Packages:\n"
            << "total: " << packages_total << ", valid: " << packages_valid;
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
        case state::read_id:
            read_id();
            break;
        case state::read_data:
            read_data();
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
        while (current_state != state::exiting)
        {
            step();
        }
    }
}
