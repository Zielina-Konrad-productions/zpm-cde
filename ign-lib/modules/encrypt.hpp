#pragma once
#include "file.hpp"

#include "../third_party/monocypher/monocypher.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ign::file {

    inline constexpr bool encryption_available = true;

    namespace detail {

        inline constexpr std::size_t encryption_chunk_size = 64 * 1024;
        inline constexpr std::size_t encryption_salt_size = 16;
        inline constexpr std::size_t encryption_nonce_size = 24;
        inline constexpr std::size_t encryption_mac_size = 16;
        inline constexpr std::uint32_t encryption_kdf_algorithm = 2;
        inline constexpr std::uint32_t encryption_kdf_blocks = 32 * 1024;
        inline constexpr std::uint32_t encryption_kdf_passes = 3;
        inline constexpr std::array<unsigned char, 8> encryption_magic = {
            'I', 'G', 'N', 'E', 'N', 'C', '2', '\n'
        };

        inline void store32_le(unsigned char out[4], std::uint32_t value) {
            out[0] = static_cast<unsigned char>(value);
            out[1] = static_cast<unsigned char>(value >> 8);
            out[2] = static_cast<unsigned char>(value >> 16);
            out[3] = static_cast<unsigned char>(value >> 24);
        }

        inline void store64_le(unsigned char out[8], std::uint64_t value) {
            for (std::size_t i = 0; i < 8; ++i) {
                out[i] = static_cast<unsigned char>(value >> (i * 8));
            }
        }

        inline std::uint32_t load32_le(const unsigned char in[4]) {
            return static_cast<std::uint32_t>(in[0]) |
                (static_cast<std::uint32_t>(in[1]) << 8) |
                (static_cast<std::uint32_t>(in[2]) << 16) |
                (static_cast<std::uint32_t>(in[3]) << 24);
        }

        inline ign::status create_parent_directories(
            const std::filesystem::path& path) {
            const std::filesystem::path fullpath =
                ign::detail::expand_user_path(path);
            if (!fullpath.has_parent_path()) return ign::status::ok;

            std::error_code ec;
            std::filesystem::create_directories(fullpath.parent_path(), ec);
            return ec ? error_result(ec) : ign::status::ok;
        }

        inline bool equivalent_paths(
            const std::filesystem::path& first,
            const std::filesystem::path& second) {
            const std::filesystem::path first_path =
                ign::detail::expand_user_path(first);
            const std::filesystem::path second_path =
                ign::detail::expand_user_path(second);
            std::error_code ec;
            const bool result =
                std::filesystem::equivalent(first_path, second_path, ec);
            return !ec && result;
        }

        inline ign::status random_bytes(
            unsigned char* output,
            std::size_t size) {
            if (size == 0) return ign::status::ok;

#if defined(_WIN32)
            using rtl_gen_random_t = BOOLEAN (APIENTRY *)(PVOID, ULONG);

            HMODULE advapi = LoadLibraryA("advapi32.dll");
            if (advapi == nullptr) {
                return ign::status::error;
            }

            const auto rtl_gen_random =
                reinterpret_cast<rtl_gen_random_t>(
                    GetProcAddress(advapi, "SystemFunction036"));

            if (rtl_gen_random == nullptr) {
                FreeLibrary(advapi);
                return ign::status::error;
            }

            std::size_t offset = 0;
            bool ok = true;

            while (offset < size) {
                const std::size_t remaining = size - offset;
                const ULONG chunk = remaining > 65536
                    ? static_cast<ULONG>(65536)
                    : static_cast<ULONG>(remaining);

                if (!rtl_gen_random(output + offset, chunk)) {
                    ok = false;
                    break;
                }

                offset += chunk;
            }

            FreeLibrary(advapi);

            return ok ? ign::status::ok : ign::status::error;
#else
            errno = 0;
            std::ifstream random_file("/dev/urandom", std::ios::binary);
            if (!random_file) return stream_error_result();

            random_file.read(
                reinterpret_cast<char*>(output),
                static_cast<std::streamsize>(size));

            if (random_file.gcount() != static_cast<std::streamsize>(size)) {
                return random_file.eof()
                    ? ign::status::error
                    : stream_error_result();
            }

            return ign::status::ok;
#endif
        }

        inline std::string random_hex_suffix() {
            unsigned char random_data[8] = {};
            if (random_bytes(random_data, sizeof random_data) !=
                ign::status::ok) {
                return "fallback";
            }

            static constexpr char hex[] = "0123456789abcdef";
            std::string suffix;
            suffix.reserve(16);

            for (const unsigned char value : random_data) {
                suffix.push_back(hex[value >> 4]);
                suffix.push_back(hex[value & 0x0f]);
            }

            return suffix;
        }

        inline std::filesystem::path temporary_encryption_path(
            const std::filesystem::path& destination) {
            std::filesystem::path temporary = destination;
            temporary += ".ign-tmp-";
            temporary += random_hex_suffix();
            return temporary;
        }

        inline std::filesystem::path default_encrypted_path(
            const std::filesystem::path& source) {
            std::filesystem::path destination = source;
            destination += ".ign";
            return destination;
        }

        inline std::filesystem::path default_decrypted_path(
            const std::filesystem::path& source) {
            const std::string text = source.string();

            if (text.size() > 4 &&
                text.compare(text.size() - 4, 4, ".ign") == 0) {
                return std::filesystem::path(text.substr(0, text.size() - 4));
            }

            std::filesystem::path destination = source;
            destination += ".dec";
            return destination;
        }

        inline ign::status copy_temporary_file(
            const std::filesystem::path& temporary,
            const std::filesystem::path& destination) {
            std::error_code ec;
            std::filesystem::copy_file(
                temporary,
                destination,
                std::filesystem::copy_options::overwrite_existing,
                ec);

            std::error_code remove_ec;
            std::filesystem::remove(temporary, remove_ec);

            return ec ? error_result(ec) : ign::status::ok;
        }

        inline void remove_temporary_file(
            const std::filesystem::path& temporary) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
        }

        inline void close_and_remove_temporary_file(
            std::ofstream& output,
            const std::filesystem::path& temporary) {
            output.close();
            remove_temporary_file(temporary);
        }

        inline ign::status write_bytes(
            std::ofstream& output,
            const unsigned char* data,
            std::size_t size) {
            output.write(reinterpret_cast<const char*>(data),
                         static_cast<std::streamsize>(size));
            return output ? ign::status::ok : stream_error_result();
        }

        inline ign::status read_bytes(
            std::ifstream& input,
            unsigned char* data,
            std::size_t size) {
            input.read(reinterpret_cast<char*>(data),
                       static_cast<std::streamsize>(size));

            if (input.gcount() != static_cast<std::streamsize>(size)) {
                return input.eof()
                    ? ign::status::error
                    : stream_error_result();
            }

            return ign::status::ok;
        }

        inline std::vector<unsigned char> make_encryption_header(
            const std::array<unsigned char, encryption_salt_size>& salt,
            const std::array<unsigned char, encryption_nonce_size>& nonce,
            std::uint32_t chunk_size,
            std::uint32_t kdf_blocks,
            std::uint32_t kdf_passes) {
            std::vector<unsigned char> header;
            header.reserve(
                encryption_magic.size() +
                salt.size() +
                nonce.size() +
                12);

            header.insert(
                header.end(), encryption_magic.begin(), encryption_magic.end());
            header.insert(header.end(), salt.begin(), salt.end());
            header.insert(header.end(), nonce.begin(), nonce.end());

            unsigned char value[4] = {};
            store32_le(value, chunk_size);
            header.insert(header.end(), value, value + 4);
            store32_le(value, kdf_blocks);
            header.insert(header.end(), value, value + 4);
            store32_le(value, kdf_passes);
            header.insert(header.end(), value, value + 4);

            return header;
        }

        inline ign::status read_encryption_header(
            std::ifstream& input,
            std::array<unsigned char, encryption_salt_size>& salt,
            std::array<unsigned char, encryption_nonce_size>& nonce,
            std::uint32_t& chunk_size,
            std::uint32_t& kdf_blocks,
            std::uint32_t& kdf_passes,
            std::vector<unsigned char>& header) {
            std::array<unsigned char, encryption_magic.size()> magic = {};
            ign::status result = read_bytes(input, magic.data(), magic.size());
            if (result != ign::status::ok) return result;
            if (magic != encryption_magic) return ign::status::error;

            result = read_bytes(input, salt.data(), salt.size());
            if (result != ign::status::ok) return result;

            result = read_bytes(input, nonce.data(), nonce.size());
            if (result != ign::status::ok) return result;

            unsigned char value[4] = {};
            result = read_bytes(input, value, sizeof value);
            if (result != ign::status::ok) return result;
            chunk_size = load32_le(value);

            result = read_bytes(input, value, sizeof value);
            if (result != ign::status::ok) return result;
            kdf_blocks = load32_le(value);

            result = read_bytes(input, value, sizeof value);
            if (result != ign::status::ok) return result;
            kdf_passes = load32_le(value);

            if (chunk_size == 0 ||
                chunk_size > encryption_chunk_size ||
                kdf_blocks < 8 ||
                kdf_passes == 0) {
                return ign::status::error;
            }

            header = make_encryption_header(
                salt, nonce, chunk_size, kdf_blocks, kdf_passes);
            return ign::status::ok;
        }

        class encryption_key {
        public:
            encryption_key() = default;
            encryption_key(const encryption_key&) = delete;
            encryption_key& operator=(const encryption_key&) = delete;

            ~encryption_key() {
                ign_monocypher::crypto_wipe(data_.data(), data_.size());
            }

            unsigned char* data() {
                return data_.data();
            }

            const unsigned char* data() const {
                return data_.data();
            }

        private:
            std::array<unsigned char, 32> data_ = {};
        };

        inline ign::status derive_encryption_key(
            encryption_key& key,
            std::string_view password,
            const std::array<unsigned char, encryption_salt_size>& salt,
            std::uint32_t kdf_blocks,
            std::uint32_t kdf_passes) {
            if (password.empty()) return ign::status::error;
            if (kdf_blocks < 8 || kdf_passes == 0) {
                return ign::status::error;
            }

            const std::size_t work_size =
                static_cast<std::size_t>(kdf_blocks) * 1024;
            std::vector<std::uint64_t> work((work_size + 7) / 8);

            ign_monocypher::crypto_argon2_config config = {
                encryption_kdf_algorithm,
                kdf_blocks,
                kdf_passes,
                1
            };

            ign_monocypher::crypto_argon2_inputs inputs = {
                reinterpret_cast<const unsigned char*>(password.data()),
                salt.data(),
                static_cast<std::uint32_t>(password.size()),
                static_cast<std::uint32_t>(salt.size())
            };

            ign_monocypher::crypto_argon2(
                key.data(),
                32,
                work.data(),
                config,
                inputs,
                ign_monocypher::crypto_argon2_no_extras);

            ign_monocypher::crypto_wipe(work.data(), work.size() * 8);
            return ign::status::ok;
        }

        inline std::array<unsigned char, encryption_nonce_size> chunk_nonce(
            const std::array<unsigned char, encryption_nonce_size>& nonce,
            std::uint64_t chunk_index) {
            std::array<unsigned char, encryption_nonce_size> chunk = nonce;
            store64_le(chunk.data() + 16, chunk_index);
            return chunk;
        }

        inline std::vector<unsigned char> chunk_ad(
            const std::vector<unsigned char>& header,
            std::uint64_t chunk_index,
            std::uint32_t chunk_size,
            unsigned char flags) {
            std::vector<unsigned char> ad = header;
            unsigned char index_bytes[8] = {};
            unsigned char size_bytes[4] = {};

            store64_le(index_bytes, chunk_index);
            store32_le(size_bytes, chunk_size);

            ad.insert(ad.end(), index_bytes, index_bytes + 8);
            ad.insert(ad.end(), size_bytes, size_bytes + 4);
            ad.push_back(flags);
            return ad;
        }

    }

    // Encrypts source into destination using a password.
    inline ign::status encrypt(
        const std::filesystem::path& source,
        const std::filesystem::path& destination,
        std::string_view password) {
        const std::filesystem::path source_path =
            ign::detail::expand_user_path(source);
        const std::filesystem::path destination_path =
            ign::detail::expand_user_path(destination);

        if (detail::equivalent_paths(source_path, destination_path)) {
            return ign::status::error;
        }

        ign::status result = detail::create_parent_directories(
            destination_path);
        if (result != ign::status::ok) return result;

        const std::filesystem::path temporary =
            detail::temporary_encryption_path(destination_path);

        errno = 0;
        std::ifstream input(source_path, std::ios::in | std::ios::binary);
        if (!input) return detail::stream_error_result();

        errno = 0;
        std::ofstream output(
            temporary,
            std::ios::out | std::ios::binary | std::ios::trunc);
        if (!output) return detail::stream_error_result();

        std::array<unsigned char, detail::encryption_salt_size> salt = {};
        std::array<unsigned char, detail::encryption_nonce_size> nonce = {};

        result = detail::random_bytes(salt.data(), salt.size());
        if (result != ign::status::ok) {
            detail::close_and_remove_temporary_file(output, temporary);
            return result;
        }

        result = detail::random_bytes(nonce.data(), nonce.size());
        if (result != ign::status::ok) {
            detail::close_and_remove_temporary_file(output, temporary);
            return result;
        }

        const std::vector<unsigned char> header =
            detail::make_encryption_header(
                salt,
                nonce,
                static_cast<std::uint32_t>(detail::encryption_chunk_size),
                detail::encryption_kdf_blocks,
                detail::encryption_kdf_passes);

        detail::encryption_key key;
        result = detail::derive_encryption_key(
            key,
            password,
            salt,
            detail::encryption_kdf_blocks,
            detail::encryption_kdf_passes);
        if (result != ign::status::ok) {
            detail::close_and_remove_temporary_file(output, temporary);
            return result;
        }

        result = detail::write_bytes(output, header.data(), header.size());
        if (result != ign::status::ok) {
            detail::close_and_remove_temporary_file(output, temporary);
            return result;
        }

        std::vector<unsigned char> plain(detail::encryption_chunk_size);
        std::vector<unsigned char> cipher(detail::encryption_chunk_size);
        std::uint64_t chunk_index = 0;

        while (true) {
            input.read(
                reinterpret_cast<char*>(plain.data()),
                static_cast<std::streamsize>(plain.size()));
            const std::streamsize read_size = input.gcount();

            if (read_size < 0 ||
                read_size >
                    static_cast<std::streamsize>(
                        detail::encryption_chunk_size)) {
                detail::close_and_remove_temporary_file(output, temporary);
                return ign::status::error;
            }

            if (!input && !input.eof()) {
                detail::close_and_remove_temporary_file(output, temporary);
                return detail::stream_error_result();
            }

            const bool is_final = input.eof();
            const unsigned char flags = is_final ? 1 : 0;
            const std::uint32_t chunk_size =
                static_cast<std::uint32_t>(read_size);
            unsigned char size_bytes[4] = {};
            unsigned char mac[detail::encryption_mac_size] = {};

            detail::store32_le(size_bytes, chunk_size);
            result = detail::write_bytes(output, size_bytes, sizeof size_bytes);
            if (result != ign::status::ok) {
                detail::close_and_remove_temporary_file(output, temporary);
                return result;
            }

            result = detail::write_bytes(output, &flags, 1);
            if (result != ign::status::ok) {
                detail::close_and_remove_temporary_file(output, temporary);
                return result;
            }

            const auto current_nonce =
                detail::chunk_nonce(nonce, chunk_index);
            const std::vector<unsigned char> ad =
                detail::chunk_ad(header, chunk_index, chunk_size, flags);

            ign_monocypher::crypto_aead_lock(
                cipher.data(),
                mac,
                key.data(),
                current_nonce.data(),
                ad.data(),
                ad.size(),
                plain.data(),
                chunk_size);

            result = detail::write_bytes(
                output, mac, detail::encryption_mac_size);
            if (result != ign::status::ok) {
                detail::close_and_remove_temporary_file(output, temporary);
                return result;
            }

            result = detail::write_bytes(output, cipher.data(), chunk_size);
            if (result != ign::status::ok) {
                detail::close_and_remove_temporary_file(output, temporary);
                return result;
            }

            if (is_final) break;
            if (chunk_index == std::numeric_limits<std::uint64_t>::max()) {
                detail::close_and_remove_temporary_file(output, temporary);
                return ign::status::error;
            }
            ++chunk_index;
        }

        output.close();
        if (!output) {
            detail::remove_temporary_file(temporary);
            return detail::stream_error_result();
        }

        return detail::copy_temporary_file(temporary, destination_path);
    }

    // Encrypts source into source + ".ign".
    // Usage example: ign::file::encrypt("secret.txt", password);
    inline ign::status encrypt(
        const std::filesystem::path& source,
        std::string_view password) {
        return encrypt(source, detail::default_encrypted_path(source), password);
    }

    // Decrypts source into destination using the same password that was used
    // for encrypt().
    inline ign::status decrypt(
        const std::filesystem::path& source,
        const std::filesystem::path& destination,
        std::string_view password) {
        const std::filesystem::path source_path =
            ign::detail::expand_user_path(source);
        const std::filesystem::path destination_path =
            ign::detail::expand_user_path(destination);

        if (detail::equivalent_paths(source_path, destination_path)) {
            return ign::status::error;
        }

        ign::status result = detail::create_parent_directories(
            destination_path);
        if (result != ign::status::ok) return result;

        const std::filesystem::path temporary =
            detail::temporary_encryption_path(destination_path);

        errno = 0;
        std::ifstream input(source_path, std::ios::in | std::ios::binary);
        if (!input) return detail::stream_error_result();

        std::array<unsigned char, detail::encryption_salt_size> salt = {};
        std::array<unsigned char, detail::encryption_nonce_size> nonce = {};
        std::uint32_t chunk_size_from_header = 0;
        std::uint32_t kdf_blocks = 0;
        std::uint32_t kdf_passes = 0;
        std::vector<unsigned char> header;

        result = detail::read_encryption_header(
            input,
            salt,
            nonce,
            chunk_size_from_header,
            kdf_blocks,
            kdf_passes,
            header);
        if (result != ign::status::ok) return result;

        detail::encryption_key key;
        result = detail::derive_encryption_key(
            key, password, salt, kdf_blocks, kdf_passes);
        if (result != ign::status::ok) return result;

        errno = 0;
        std::ofstream output(
            temporary,
            std::ios::out | std::ios::binary | std::ios::trunc);
        if (!output) return detail::stream_error_result();

        std::vector<unsigned char> plain(chunk_size_from_header);
        std::vector<unsigned char> cipher(chunk_size_from_header);
        std::uint64_t chunk_index = 0;
        bool saw_final = false;

        while (!saw_final) {
            unsigned char size_bytes[4] = {};
            unsigned char flags = 0;
            unsigned char mac[detail::encryption_mac_size] = {};

            result = detail::read_bytes(input, size_bytes, sizeof size_bytes);
            if (result != ign::status::ok) {
                detail::close_and_remove_temporary_file(output, temporary);
                return result;
            }

            result = detail::read_bytes(input, &flags, 1);
            if (result != ign::status::ok) {
                detail::close_and_remove_temporary_file(output, temporary);
                return result;
            }

            const std::uint32_t chunk_size =
                detail::load32_le(size_bytes);
            if (chunk_size > chunk_size_from_header || (flags & ~1) != 0) {
                detail::close_and_remove_temporary_file(output, temporary);
                return ign::status::error;
            }

            result = detail::read_bytes(
                input, mac, detail::encryption_mac_size);
            if (result != ign::status::ok) {
                detail::close_and_remove_temporary_file(output, temporary);
                return result;
            }

            result = detail::read_bytes(input, cipher.data(), chunk_size);
            if (result != ign::status::ok) {
                detail::close_and_remove_temporary_file(output, temporary);
                return result;
            }

            const auto current_nonce =
                detail::chunk_nonce(nonce, chunk_index);
            const std::vector<unsigned char> ad =
                detail::chunk_ad(header, chunk_index, chunk_size, flags);

            if (ign_monocypher::crypto_aead_unlock(
                    plain.data(),
                    mac,
                    key.data(),
                    current_nonce.data(),
                    ad.data(),
                    ad.size(),
                    cipher.data(),
                    chunk_size) != 0) {
                detail::close_and_remove_temporary_file(output, temporary);
                return ign::status::error;
            }

            result = detail::write_bytes(output, plain.data(), chunk_size);
            if (result != ign::status::ok) {
                detail::close_and_remove_temporary_file(output, temporary);
                return result;
            }

            saw_final = (flags & 1) != 0;
            if (!saw_final) {
                if (chunk_index ==
                    std::numeric_limits<std::uint64_t>::max()) {
                    detail::close_and_remove_temporary_file(output, temporary);
                    return ign::status::error;
                }
                ++chunk_index;
            }
        }

        char extra = 0;
        if (input.get(extra)) {
            detail::close_and_remove_temporary_file(output, temporary);
            return ign::status::error;
        }

        if (!input.eof()) {
            detail::close_and_remove_temporary_file(output, temporary);
            return detail::stream_error_result();
        }

        output.close();
        if (!output) {
            detail::remove_temporary_file(temporary);
            return detail::stream_error_result();
        }

        return detail::copy_temporary_file(temporary, destination_path);
    }

    // Decrypts source into a default destination.
    // "secret.txt.ign" becomes "secret.txt"; other names get ".dec".
    inline ign::status decrypt(
        const std::filesystem::path& source,
        std::string_view password) {
        return decrypt(source, detail::default_decrypted_path(source), password);
    }

}
