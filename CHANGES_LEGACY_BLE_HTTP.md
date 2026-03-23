# 레거시 BLE/HTTP 지원 및 관련 변경 사항 요약

이 문서는 2.08 폰 호환(레거시)을 위한 BLE·HTTP·메타 구성 변경과, 2차 메타를 BLE 전용으로 분리한 수정, 그리고 문서 추가분을 정리한 것입니다.  
**이미 적용된 내용**이므로, 저장은 각 파일이 편집 도구로 수정된 시점에 반영되어 있습니다.

---

## 1. 새로 추가된 파일

| 경로 | 설명 |
|------|------|
| `main/core/firmware_upload/fw_ble_legacy_context.h` | BLE 전용 2차 메타: set_type, set_expected_size, set_file_name, get_snapshot API |
| `main/core/firmware_upload/fw_ble_legacy_context.c` | 위 API 구현 (TCP 무관, 내부 static 저장) |
| `main/docs/OTA_SAVE_RUN_FLOW_VERIFY.md` | 1·2·3차 후 save_only에 따른 저장·run 흐름 검증, 2.08 vs Geobot 정리 |

---

## 2. 수정된 파일 — BLE / 2차 메타

| 파일 | 변경 내용 |
|------|-----------|
| `main/protocol/ble_cmd_parser.c` | `#include "core/firmware_upload/fw_ble_legacy_context.h"` 추가. 0x0055 수신 시 `fw_ble_legacy_set_type`, `fw_ble_legacy_set_file_name` 호출. 0x0040 수신 시 `fw_ble_legacy_set_expected_size` 호출. 0x0055에서 `tcp_server_set_pending_file` 제거(2차는 BLE 전용으로 분리). |

---

## 3. 수정된 파일 — HTTP 핸들러 (1·2·3차 메타)

| 파일 | 변경 내용 |
|------|-----------|
| `main/base/net/http/sub/rest_sdmmc_fw_run.c` | 파일 상단 주석: 레거시 폰 호환 1·2·3차 메타 구성 설명. 2차: `tcp_server_get_pending_snapshot` → **`fw_ble_legacy_get_snapshot`** 로 변경(BLE 전용). `#include "core/firmware_upload/fw_ble_legacy_context.h"` 추가. |
| `main/base/net/http/sub/rest_sdmmc_fw_upload_run_mobile.c` | 파일 상단 주석: 동일 1·2·3차 설명. 2차: **`fw_ble_legacy_get_snapshot`** 사용. `#include "base/net/tcp/tcp_server.h"` 제거, `#include "core/firmware_upload/fw_ble_legacy_context.h"` 추가. |

---

## 4. 수정된 파일 — tcp_server

| 파일 | 변경 내용 |
|------|-----------|
| `main/base/net/tcp/tcp_server.h` | `tcp_server_get_pending_snapshot` 선언 제거. |
| `main/base/net/tcp/tcp_server.c` | `tcp_server_get_pending_snapshot` 구현 제거. |

---

## 5. 수정된 파일 — HTTP 서버 등록

| 파일 | 변경 내용 |
|------|-----------|
| `main/base/net/http/http_server.c` | `/fw_upload_run`, `/fw_run` 등록 직전 주석: "레거시 폰 호환 시 메타는 1차 JSON → 2차 BLE → 3차 type 기본값으로 구성 (각 핸들러 주석 참고)". |

---

## 6. 수정된 파일 — 빌드

| 파일 | 변경 내용 |
|------|-----------|
| `main/CMakeLists.txt` | `core/firmware_upload/fw_ble_legacy_context.c` 를 SRCS에 추가. |

---

## 7. 문서만 추가/수정된 파일 (이전 대화에서)

| 파일 | 변경 내용 |
|------|-----------|
| `main/base/net/http/sub/rest_sdmmc_fw_run.c` | 최상단 블록 주석: POST /fw_run, 레거시 폰 1·2·3차 메타 구성 설명. |
| `main/base/net/http/sub/rest_sdmmc_fw_upload_run_mobile.c` | 최상단 블록 주석: POST /fw_upload_run, 레거시 폰 1·2·3차 메타 구성 설명. |
| `main/docs/OTA_SAVE_RUN_FLOW_VERIFY.md` | 5절 추가: 2.08 OTA와의 동작 보장(암호화·형식 차이, 같은 폰 → miri-tool 정상 동작 정리). |

---

## 8. 저장 상태

- 위 목록의 **모든 수정은 이미 해당 파일에 적용된 상태**입니다.
- IDE에서 해당 파일들을 열어 두었다면 **저장(Ctrl+S / Cmd+S)** 만 해 주시면, 디스크에 최종 반영됩니다.
- 프로젝트 전체 저장: **Ctrl+K S** (VS Code/Cursor) 또는 **File → Save All**.

이 문서는 변경 이력을 남기기 위한 요약입니다.
