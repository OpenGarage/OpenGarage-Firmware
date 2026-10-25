#include "firmware_update.h"

#if defined(ARDUINO)

#include "OpenGarage.h"
#include "defines.h"
#include <ArduinoJson.h>
#include "firmware_update_public_key.h"

#if defined(ESP8266)
	#include <Updater.h>
	#include <StackThunk.h>
	#include <umm_malloc/umm_heap_select.h>
	#include <bearssl/bearssl.h>
#else
#error "Implement and validate the OG ESP32-C3 updater before enabling this target"
#endif

#ifndef FIRMWARE_UPDATE_BASE_URL
	#define FIRMWARE_UPDATE_BASE_URL "https://firmware.opengarage.io"
#endif

#define FIRMWARE_UPDATE_DESCRIPTOR_LIMIT 1024
#define FIRMWARE_UPDATE_TOKEN_TTL 300000UL

extern void restart_in(uint32_t ms);

FirmwareUpdateService firmware_update;

static bool update_sha256_active = false;

static br_sha256_context update_sha256;

static void sha256_begin() {
	br_sha256_init(&update_sha256);
	update_sha256_active = true;
}

static void sha256_add(const uint8_t *data, size_t length) {
	br_sha256_update(&update_sha256, data, length);
}

static void sha256_finish(uint8_t digest[32]) {
	br_sha256_out(&update_sha256, digest);
	update_sha256_active = false;
}

static void sha256_cancel() {
	update_sha256_active = false;
}

static void sha256_buffer(const uint8_t *data, size_t length, uint8_t digest[32]) {
	sha256_begin();
	sha256_add(data, length);
	sha256_finish(digest);
}

extern "C" uint32_t firmware_update_verify_signature(const uint8_t *digest,
	const uint8_t *signature, size_t length) {
	br_ec_public_key key = {BR_EC_secp256r1,
		const_cast<unsigned char *>(FW_UPDATE_PUBLIC_KEY), sizeof(FW_UPDATE_PUBLIC_KEY)};
	br_ecdsa_vrfy verify = br_ecdsa_vrfy_asn1_get_default();
	return verify && verify(br_ec_get_default(), digest, 32, &key, signature, length) == 1;
}

make_stack_thunk(firmware_update_verify_signature);
extern "C" uint32_t thunk_firmware_update_verify_signature(const uint8_t *digest,
	const uint8_t *signature, size_t length);

static bool verify_release_signature(const uint8_t digest[32], const uint8_t *signature, size_t length) {
	// Core aborts if the first 6200-byte DRAM thunk allocation fails.
	// Conservative preflight; no dynamic allocation/yield between check and acquire.
	HeapSelectDram dram;
	if (!stack_thunk_get_refcnt() && ESP.getMaxFreeBlockSize() < 8192) return false;
	stack_thunk_add_ref();
	bool valid = thunk_firmware_update_verify_signature(digest, signature, length) == 1;
	stack_thunk_del_ref();
	return valid;
}

static bool hex_to_bytes(const char *input, uint8_t *output, size_t capacity, size_t &length) {
	length = 0;
	while (input && *input && *input != '\r' && *input != '\n') {
		char hi = *input++;
		if (!*input || length >= capacity) return false;
		char lo = *input++;
		auto nibble = [](char c) -> int8_t {
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			return -1;
		};
		int8_t h = nibble(hi);
		int8_t l = nibble(lo);
		if (h < 0 || l < 0) return false;
		output[length++] = (uint8_t)((h << 4) | l);
	}
	return true;
}

static bool parse_sha256(const char *input, uint8_t output[32]) {
	if (!input || strlen(input) != 64) return false;
	size_t length = 0;
	return hex_to_bytes(input, output, 32, length) && length == 32;
}

bool FirmwareUpdateService::configured() const {
	return FW_UPDATE_PUBLIC_KEY_CONFIGURED == 1;
}

const char *FirmwareUpdateService::target() const {
	return "og-esp8266-4m-v1";
}

const char *FirmwareUpdateService::file_extension() const {
	return ".bin";
}

const char *FirmwareUpdateService::base_url() const {
	return FIRMWARE_UPDATE_BASE_URL;
}

void FirmwareUpdateService::make_token(char token_out[17]) {
	uint32_t a = ESP.random();
	uint32_t b = ESP.random();
	snprintf(token_out, 17, "%08lx%08lx", (unsigned long)a, (unsigned long)b);
}

bool FirmwareUpdateService::token_valid(const char *token) const {
 return _phase == FirmwareUpdatePhase::Prepared && token && _upload_token[0] &&
   strcmp(token, _upload_token)==0 && (int32_t)(millis()-_upload_token_expires)<0;
}

bool FirmwareUpdateService::prepare_verified(const uint8_t *descriptor, size_t descriptor_length,
	const char *signature_hex, const char *release_id, bool allow_downgrade,
	char upload_token_out[17]) {
	if (busy() || _phase == FirmwareUpdatePhase::Success) return false;
	reset_transfer();
	if (!configured()) {
		fail("Online updates are not configured");
		return false;
	}
	if (!descriptor || !descriptor_length || descriptor_length > FIRMWARE_UPDATE_DESCRIPTOR_LIMIT ||
		!release_id || !release_id[0]) {
		fail("Signed release request is invalid");
		return false;
	}

	if (!stack_thunk_get_refcnt() && ESP.getMaxFreeBlockSize() < 8192) {
		fail("Not enough contiguous memory to verify release; retry later"); return false;
	}
	uint8_t signature[72];
	size_t signature_length = 0;
	uint8_t digest[32];
	sha256_buffer(descriptor, descriptor_length, digest);
	if (!hex_to_bytes(signature_hex, signature, sizeof(signature), signature_length) ||
		signature_length < 64 || !verify_release_signature(digest, signature, signature_length)) {
		fail("Release signature is invalid");
		return false;
	}

	ArduinoJson::JsonDocument doc;
	ArduinoJson::DeserializationError error =
		ArduinoJson::deserializeJson(doc, descriptor, descriptor_length);
	if (error || doc["schema"].as<uint8_t>() != 1 ||
        strcmp(doc["product"] | "", "opengarage-stock") != 0) {
		fail("Release descriptor format is invalid");
		return false;
	}
	const char *descriptor_id = doc["id"];
	if (!descriptor_id || strcmp(descriptor_id, release_id) != 0) {
		fail("Signed release does not match the requested release");
		return false;
	}

	uint32_t version = doc["version"] | 0;
	bool newer = version > OG_FWV;
	if ((version < 126 || version > 255) || (!newer && !allow_downgrade)) {
		fail("Selected firmware is not newer than this build");
		return false;
	}

	ArduinoJson::JsonObject artifact = doc["targets"][target()].as<ArduinoJson::JsonObject>();
	const char *path = artifact["file"];
	size_t path_length = path ? strlen(path) : 0;
	size_t extension_length = strlen(file_extension());
	uint32_t size = artifact["size"] | 0;
	uint32_t min_flash = artifact["min_flash"] | 0;
	if (artifact.isNull() || !path || strncmp(path, "/v1/releases/", 13) != 0 ||
		strstr(path, "..") || strchr(path, '?') || path_length < extension_length ||
		strcmp(path + path_length - extension_length, file_extension()) != 0 ||
		!parse_sha256(artifact["sha256"], _expected_sha256) || size < 1024 ||
		size > ESP.getFreeSketchSpace() || (min_flash != 4194304 || ESP.getFlashChipRealSize() < min_flash)) {
		fail("Signed release contains an incompatible entry");
		return false;
	}

	_bytes_done = 0;
	_bytes_total = size;
	make_token(_upload_token);
	strcpy(upload_token_out, _upload_token);
	_upload_token_expires = millis() + FIRMWARE_UPDATE_TOKEN_TTL;
	_phase = FirmwareUpdatePhase::Prepared;
	strcpy(_message, "Ready to receive verified firmware");
	return true;
}

bool FirmwareUpdateService::valid_image_header(const uint8_t header[16]) const {
	if (header[0] != 0xE9 || header[1] == 0 || header[1] > 16) return false;
	uint32_t entry = (uint32_t)header[4] | ((uint32_t)header[5] << 8) |
		((uint32_t)header[6] << 16) | ((uint32_t)header[7] << 24);
	uint16_t chip_id = (uint16_t)header[12] | ((uint16_t)header[13] << 8);
	return chip_id != 0x000D && entry >= 0x40000000UL && entry < 0x40300000UL;
}

bool FirmwareUpdateService::begin_image(const uint8_t header[16], uint32_t size) {
	if (!valid_image_header(header)) {
		fail("Firmware file is for a different hardware target");
		return false;
	}
	if (!Update.begin(size)) {
		fail("Not enough OTA space for this firmware");
		return false;
	}
	_update_started = true;
	return true;
}

bool FirmwareUpdateService::begin_upload(const char *filename, bool verified, uint32_t size,
	bool unsized_manual) {
	const char *dot = filename ? strrchr(filename, '.') : nullptr;
	if (!dot || strcmp(dot, file_extension()) != 0) {
		fail("Firmware filename has the wrong extension");
		return false;
	}
	_bytes_done = 0;
	_image_header_len = 0;
	_tail_pending = false;
	_update_started = false;
	_manual = !verified;
	_unsized_manual = unsized_manual;
	_reboot_at = 0;
	if (!verified) _bytes_total = size;
	sha256_begin();
	_phase = FirmwareUpdatePhase::Uploading;
	strcpy(_message, verified ? "Uploading verified firmware" : "Receiving firmware upload");
	return true;
}

bool FirmwareUpdateService::begin_verified(const char *filename, const char *upload_token) {
	if (busy() || _phase == FirmwareUpdatePhase::Success) return false;
	bool authorized = _phase == FirmwareUpdatePhase::Prepared && upload_token && _upload_token[0] &&
		strcmp(upload_token, _upload_token) == 0 &&
		(int32_t)((uint32_t)millis() - _upload_token_expires) < 0;
	_upload_token[0] = 0;
	_upload_token_expires = 0;
	if (!authorized) {
		fail("Verified firmware upload is not authorized");
		return false;
	}
	return begin_upload(filename, true);
}

bool FirmwareUpdateService::begin_manual(const char *filename, uint32_t size, const char *sha256_hex) {
	if (busy() || _phase == FirmwareUpdatePhase::Success) return false;
	reset_transfer();
	if (!size || !parse_sha256(sha256_hex, _expected_sha256)) {
		fail("Firmware upload size or checksum is invalid");
		return false;
	}
	return begin_upload(filename, false, size);
}

bool FirmwareUpdateService::begin_manual_file(const char *filename) {
	if (busy() || _phase == FirmwareUpdatePhase::Success) return false;
	reset_transfer();
	return begin_upload(filename, false, 0, true);
}

bool FirmwareUpdateService::write_upload(const uint8_t *data, size_t length, bool verified) {
	if (_phase != FirmwareUpdatePhase::Uploading || _manual == verified || !data || !length) return false;
	const uint32_t free_space = ESP.getFreeSketchSpace();
	const uint32_t capacity = free_space > 0x1000 ? (free_space-0x1000)&0xFFFFF000 : 0;
	if (_unsized_manual && (_bytes_done > capacity || length > capacity-_bytes_done)) {
		fail("Firmware exceeds OTA space"); return false;
	}
	if (_bytes_total && (_bytes_done > _bytes_total || length > _bytes_total - _bytes_done)) {
		fail(verified ? "Firmware upload exceeds signed size" : "Firmware upload exceeds declared size");
		return false;
	}
	sha256_add(data, length);

	size_t offset = 0;
	if (_image_header_len < sizeof(_image_header)) {
		size_t needed = sizeof(_image_header) - _image_header_len;
		size_t copied = length < needed ? length : needed;
		memcpy(_image_header + _image_header_len, data, copied);
		_image_header_len += copied;
		offset += copied;
		if (_image_header_len == sizeof(_image_header)) {
			uint32_t update_size = _bytes_total;
			if (!update_size) {
				uint32_t free_space = ESP.getFreeSketchSpace();
				if (free_space <= 0x1000UL) {
					fail("Not enough OTA space for this firmware");
					return false;
				}
				update_size = (free_space - 0x1000UL) & 0xFFFFF000UL;
			}
			if (!begin_image(_image_header, update_size)) return false;
			if (Update.write(_image_header, sizeof(_image_header)) != sizeof(_image_header)) {
				fail("Unable to write firmware upload");
				return false;
			}
		}
	}

	size_t write_length = length - offset;
	if (_unsized_manual && write_length) {
		if (_tail_pending && Update.write(&_tail_byte, 1) != 1) {
			fail("Unable to write firmware upload");
			return false;
		}
		_tail_byte = data[offset + write_length - 1];
		_tail_pending = true;
		write_length--;
	} else if (_bytes_done + length == _bytes_total && write_length) {
		_tail_byte = data[offset + write_length - 1];
		_tail_pending = true;
		write_length--;
	}
	if (write_length && Update.write(const_cast<uint8_t *>(data + offset), write_length) != write_length) {
		fail("Unable to write firmware upload");
		return false;
	}
	_bytes_done += length;
	return true;
}

bool FirmwareUpdateService::write_verified(const uint8_t *data, size_t length) {
	return write_upload(data, length, true);
}

bool FirmwareUpdateService::write_manual(const uint8_t *data, size_t length) {
	return write_upload(data, length, false);
}

bool FirmwareUpdateService::finish_upload(bool verified) {
	if (_phase != FirmwareUpdatePhase::Uploading || _manual == verified) return false;
	if (!_update_started) {
		fail("Firmware upload is incomplete");
		return false;
	}
	_phase = FirmwareUpdatePhase::Verifying;
	strcpy(_message, verified ? "Verifying firmware" : "Finalizing firmware upload");
	if ((!_unsized_manual && _bytes_done != _bytes_total) || !_tail_pending ||
		(_unsized_manual && _bytes_done < 1024)) {
		fail(verified ? "Firmware upload size does not match signed release" :
			"Firmware upload size does not match declared size");
		return false;
	}
	uint8_t actual_sha256[32];
	sha256_finish(actual_sha256);
	if (!_unsized_manual &&
		memcmp(actual_sha256, _expected_sha256, sizeof(actual_sha256)) != 0) {
		fail("Firmware checksum verification failed");
		return false;
	}
	if (Update.write(&_tail_byte, 1) != 1 || !Update.end(_unsized_manual) || Update.hasError()) {
		fail("Firmware installation failed");
		return false;
	}
	_update_started = false;
	_manual = false;
	_unsized_manual = false;
	_phase = FirmwareUpdatePhase::Success;
	strcpy(_message, "Update complete; rebooting");
	_reboot_at = millis() + 1500UL;
	return true;
}

bool FirmwareUpdateService::finish_verified() {
	return finish_upload(true);
}

bool FirmwareUpdateService::finish_manual() {
	return finish_upload(false);
}

void FirmwareUpdateService::abort_upload() {
	if (_phase == FirmwareUpdatePhase::Uploading || _phase == FirmwareUpdatePhase::Prepared)
		fail("Firmware upload was aborted");
}

void FirmwareUpdateService::loop() {
	uint32_t now = millis();
	if (_phase == FirmwareUpdatePhase::Prepared && _upload_token[0] &&
		(int32_t)(now - _upload_token_expires) >= 0) {
		fail("Prepared firmware upload expired");
	}
	if (_phase == FirmwareUpdatePhase::Success && _reboot_at &&
		(int32_t)(now - _reboot_at) >= 0) { _reboot_at = 0; restart_in(1); }
}

void FirmwareUpdateService::reset_transfer() {
	sha256_cancel();
	_upload_token[0] = 0;
	_upload_token_expires = 0;
	_bytes_done = 0;
	_bytes_total = 0;
	_image_header_len = 0;
	_tail_pending = false;
	_update_started = false;
	_manual = false;
	_unsized_manual = false;
	_reboot_at = 0;
}

void FirmwareUpdateService::fail(const char *message) {
	sha256_cancel();
	_upload_token[0] = 0;
	_upload_token_expires = 0;
	if (_update_started) {
		Update.end(false);
	}
	_update_started = false;
	_manual = false;
	_unsized_manual = false;
	_phase = FirmwareUpdatePhase::Error;
	strncpy(_message, message, sizeof(_message) - 1);
	_message[sizeof(_message) - 1] = 0;
}

const char *FirmwareUpdateService::phase_name() const {
	switch (_phase) {
	case FirmwareUpdatePhase::Idle: return "idle";
	case FirmwareUpdatePhase::Prepared: return "prepared";
	case FirmwareUpdatePhase::Uploading: return "uploading";
	case FirmwareUpdatePhase::Verifying: return "verifying";
	case FirmwareUpdatePhase::Success: return "success";
	default: return "error";
	}
}

uint8_t FirmwareUpdateService::percent() const {
	if (!_bytes_total) return 0;
	uint32_t value = (_bytes_done * 100ULL) / _bytes_total;
	return value > 100 ? 100 : (uint8_t)value;
}

bool FirmwareUpdateService::busy() const {
	return _phase == FirmwareUpdatePhase::Uploading || _phase == FirmwareUpdatePhase::Verifying;
}


#endif
