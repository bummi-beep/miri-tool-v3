MIRI-TOOL 구조 요약 (ESP‑IDF 기반)

이 프로젝트는 **base/core/app + board 프로파일** 구조를 기준으로 구성됩니다.
이 README는 빠른 이해를 위한 요약입니다.

---

## 디렉토리 구조

```
main/
  app/                  # 앱 진입점 (app_main)

  base/                 # 기본 동작(통신/저장소/시스템)
    system/             # NVS/로그/이벤트 등 공통 초기화
    net/                # esp_netif 등 네트워크 기반
      http/             # HTTP 서버(REST 핸들러)
      tcp/              # TCP 서버(필요 시)
    ble/                # BLE 스택/광고/서비스 등록
    wifi/               # SoftAP/STA 초기화
    sdmmc/              # SDMMC 마운트, FATFS 연동
    spiflash_fs/         # SPI Flash FATFS 마운트
    runtime_config/      # 런타임 설정 저장(/c/config.json)
    http/               # HTTP 초기화(서버 start)

  core/                 # 주 동작(6가지)
    hht/                # HHT(WB100/3000) 기능
    firmware_upload/    # 펌웨어 업로드
    storage_upload/     # 저장소/파일 업로드
    esp32_update/       # ESP32 OTA
    diagnosis/          # 진단
    manual/             # 수동제어

  drivers/              # GPIO/UART/CAN 같은 저수준 I/O
  protocol/             # BLE 패킷/커맨드/프로토콜 정의
  tasks/                # RTOS 태스크 구현
  config/               # 핀맵/전역 설정
  utils/                # 공용 유틸

  board/                # 보드별 프로파일
    board_mcn8r8.c
    board_xxx.c
```

---

## 각 폴더 역할(요약)

### app/
- `app_main()` 진입점과 초기화 순서를 관리합니다.

### base/
- 시스템/통신/저장소의 **공통 기반 초기화**가 들어갑니다.
- 예: `esp_netif_init()`, `nvs_init()`, FATFS/SDMMC 마운트, HTTP 서버 시작
- `spiflash_fs/`는 `/c` FATFS, `runtime_config/`는 `/c/config.json` 관리

### core/
- 실제 기능 로직(주 동작)을 넣습니다.
- 예: HHT 제어, 펌웨어 업로드 흐름

### drivers/
- 하드웨어 접근(핀 제어, UART/SPI/CAN 등)

### protocol/
- BLE/TCP 등에서 사용하는 **패킷 구조/커맨드 파서**

### tasks/
- FreeRTOS 태스크 구현 파일

### config/
- 핀맵/전역 설정/상수 정의

### board/
- **보드별 설정만 모아둔 파일**
- 핀맵, 지원 기능, 태스크 조합을 정의

---

## 기본 동작 흐름(현재 구현)

1) `app_main()` 진입  
2) `system_init()` (로그/NVS/스파이플래시 FATFS/런타임 설정 등)  
3) SDMMC 초기화  
4) 네트워크/ BLE / Wi‑Fi / HTTP 서버 시작  
5) 공통 태스크 시작 → 역할 디스패치  

---

## 부팅 시퀀스(요약)

```
boot → app_main
  → system_init (log/nvs/spiflash_fs/runtime_config/event/console)
  → sdmmc_init
  → net_init
  → ble_init
  → wifi_init
  → http_init
  → tasks_start_common
  → role_dispatcher_init
```

## BLE 명령 처리 흐름(쉽게)

1) 폰이 BLE로 명령 전송  
2) `protocol/`에서 **cmd/payload 분리**  
3) cmd를 **Start/Stop/Action**으로 분류  
4) Start/Stop이면 **role_dispatcher가 태스크 시작/중지**  
5) Action이면 각 모듈(HHT/Storage/ESP32 Update)이 처리  
6) 결과는 **BLE Notify + 콘솔 로그**로 응답

즉, BLE는 **“명령 수신 → 역할 결정 → 작업 실행 → 결과 응답”**의 사이클입니다.

---

## CLI 명령 처리 흐름(쉽게)

1) 콘솔에서 명령 입력  
2) `cli_commands`가 입력을 BLE cmd로 매핑  
3) 내부적으로 **BLE와 동일한 처리 경로**(`ble_cmd_handle`)로 진입  
4) 역할 전환/액션 처리/결과 응답이 **BLE와 동일하게 동작**

즉, CLI는 **폰 없이도 BLE 동작을 그대로 재현**하기 위한 디버깅 경로입니다.

---

## 웹 파일 브라우저

- `http://192.168.4.1/sdmmc` : SDMMC 파일 목록/업로드/삭제/다운로드
- `http://192.168.4.1/flash` : SPI Flash(`/c`) 파일 목록/업로드/삭제/다운로드

관련 REST:
- `GET /fs_list?fs=sdmmc|flash`
- `POST /fs_upload?fs=sdmmc|flash&name=...`
- `POST /fs_delete?fs=sdmmc|flash` (JSON `{ "name": "..." }`)
- `GET /fs_download?fs=sdmmc|flash&name=...`

---

## 추가 CLI 명령(스토리지)

- `FLASHLS` : SPI Flash(`/c`) 파일 목록
- `FLASHFORMAT` : SPI Flash FATFS 포맷
- `FLASHDEL <name>` : SPI Flash 파일 삭제
