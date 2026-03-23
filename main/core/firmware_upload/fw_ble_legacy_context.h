/**
 * BLE 전용 레거시 컨텍스트 (2차 메타).
 * TCP와 무관하게, BLE(0x0055/0x0040)로 수신한 type/expected_size/default_name 만 저장하고
 * /fw_run, /fw_upload_run 핸들러에서 JSON에 없거나 비어 있을 때 보완용으로 사용.
 */
#ifndef FW_BLE_LEGACY_CONTEXT_H
#define FW_BLE_LEGACY_CONTEXT_H

#include <stdbool.h>
#include <stddef.h>

/** 0x0055 수신 시 타입 설정 (예: "S00", "A03") */
void fw_ble_legacy_set_type(const char *type);

/** 0x0040 수신 시 예상 크기 설정 */
void fw_ble_legacy_set_expected_size(size_t size);

/** 기본 파일명 설정 (0x0055/0x0040 시 자동 생성명 저장용) */
void fw_ble_legacy_set_file_name(const char *name);

/**
 * 2차 메타 스냅샷 조회. name/type/out_expected_size 는 NULL 가능.
 * @return true if at least type or name is non-empty
 */
bool fw_ble_legacy_get_snapshot(char *name, size_t name_sz, char *type, size_t type_sz, size_t *out_expected_size);

#endif /* FW_BLE_LEGACY_CONTEXT_H */
