#pragma once
#include <iostream>
#include <fstream>
#include <optional>
#include <vector>
#include <iomanip>
#include <string>

#include "state.h"
#include "crc.h"

namespace fsm_decoder
{
    using namespace std::string_literals;

    const auto input_filename = "data/data_no_error.bin"s;
    const auto output_filename = "data/output.txt"s;

    const auto expected_length = 45;
    const auto expected_sync_byte = std::byte(0xAA);
    const auto expected_id_byte = std::byte(0x87);
    const auto expected_length_byte = std::byte(expected_length);
    const auto default_width = 10;

    state current_state = state::search_sync;

    auto packages_total = 0;
    auto packages_valid = 0;
    std::ifstream in;
    std::ofstream out;
    std::vector<std::byte> data;

    std::optional<std::byte> next_byte()
    {
        if (in)
        {
            return std::byte(in.get());
        }
        return {};
    }

    std::optional<std::vector<std::byte>> next_bytes(const int length = expected_length - 2)
    {
        const auto bytes_count = in.tellg();

        auto buffer = new std::byte[length];
        in.read((char*)buffer, length);
        std::vector vec(buffer, buffer + in.tellg() - bytes_count);
        delete[] buffer;

        if (vec.size() == length)
        {
            return vec;
        }
        return {};
    }

    template <typename T>
    void output(T value, const int width = default_width)
    {
        out << std::setw(width) << value;
    }

    void search_sync()
    {
        auto b = next_byte();
        if (b.has_value())
        {
            if (b.value() == expected_sync_byte)
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
            if (b.value() == expected_sync_byte)
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
            if (b.value() == expected_length_byte)
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

            if (data[0] == expected_id_byte)
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
            auto expected_crc = (v2 << 8) | v1;
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
        out.open(output_filename);

        output("Ax");
        output("Ay");
        output("Az");

        output("Wx");
        output("Wy");
        output("Wz");

        output("Tax");
        output("Tay");
        output("Taz");

        output("Twx");
        output("Twy");
        output("Twz");

        output("S", 4);

        output("Timestamp");
        output("Status");
        output("Number");

        out << "\n";
        current_state = state::write_package;
    }

    template <typename T>
    T interpret(std::byte* arr, int start)
    {
        return *(T*)(arr + start);
    }

    void write_package()
    {
        const auto raw = data.data() + 1; // skip Id
        output(interpret<int32_t>(raw, 0)); // Ax
        output(interpret<int32_t>(raw, 4)); // Ay
        output(interpret<int32_t>(raw, 8)); // Az

        output(interpret<int32_t>(raw, 12)); // Wx
        output(interpret<int32_t>(raw, 16)); // Wy
        output(interpret<int32_t>(raw, 20)); // Wz

        output(interpret<int16_t>(raw, 24)); // Tax
        output(interpret<int16_t>(raw, 26)); // Tay
        output(interpret<int16_t>(raw, 28)); // Taz

        output(interpret<int16_t>(raw, 30)); // Twx
        output(interpret<int16_t>(raw, 32)); // Twy
        output(interpret<int16_t>(raw, 34)); // Twz

        output(interpret<int16_t>(raw, 36), 4); // S
        output(interpret<int16_t>(raw, 38)); // Timestamp

        output((uint16_t)interpret<uint8_t>(raw, 40)); // Status
        output((uint16_t)interpret<uint8_t>(raw, 41)); // Number

        out << "\n";
        current_state = state::search_sync;
    }

    void print_stats()
    {
        std::cout << "Packages total: " << packages_total <<
            ", valid: " << packages_valid <<
            ", invalid: " << (packages_total - packages_valid) << "\n";
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
        in.open(input_filename, std::ios::binary);
        while (current_state != state::exiting)
        {
            step();
        }
    }
}
