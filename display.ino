#include "sub-altar.h"

/**
 * @brief 디스플레이에 변화를 주거나 변화가 있을시 실행
 */
void DisplayCheck()
{
    while (MySerial2.available() > 0)
    {
        String nextion_string = MySerial2.readStringUntil((char)0xFF);
        while (MySerial2.available() > 0 && MySerial2.peek() == (char)0xFF)
            MySerial2.read();
        nextion_string.trim();
        if (nextion_string.length() > 0)
            NextionReceived(&nextion_string);
    }
}

/**
 * @brief 디스플레이에서 오는 Serial을 확인
 *
 * @param nextion_string Serial 문자열 데이터
 */
void NextionReceived(String *nextion_string)
{
}

/**
 * @brief get 명령으로 Nextion 숫자 변수를 읽음
 *        성공시 값 반환, 실패시 -1 반환 (0x71 응답 파싱)
 */
int NextionGetNum(const char *cmd)
{
    while (MySerial2.available()) MySerial2.read();  // 이전 잔여 데이터 제거
    sendCommand(cmd);

    uint8_t buf[8] = {0};
    int idx = 0;
    uint32_t timeout = millis() + 500;
    while (millis() < timeout && idx < 8)
    {
        if (MySerial2.available())
            buf[idx++] = MySerial2.read();
    }

    // Nextion 숫자 응답: 0x71 + 4바이트(리틀엔디안) + 0xFF 0xFF 0xFF
    if (idx >= 5 && buf[0] == 0x71)
        return (int32_t)(buf[1] | (buf[2] << 8) | (buf[3] << 16) | (buf[4] << 24));

    return -1;
}

/**
 * @brief 서버 selected_language 와 Nextion vLanguage 를 동기화 (0=EN, 1=KO)
 *        vLanguage 는 매번 다시 써서 전송이 누락돼도 다음 동기화에서 복구한다.
 *        vLanguage 는 페이지 진입 때만 화면에 반영되므로, 언어가 바뀌면
 *        page dp 로 지금 페이지를 다시 불러 새 언어로 그린다.
 */
void SyncLanguage()
{
    const char *lang = shift_machine["selected_language"];
    if (lang == nullptr || strlen(lang) == 0)
    {
        return;  // Loop 응답을 받기 전에는 언어를 모름
    }

    int target = (strcmp(lang, "EN") == 0) ? 0 : 1;
    String cmd = "pgSetting.vLanguage.val=" + String(target);
    sendCommand(cmd.c_str());

    if (nextion_language != target)
    {
        nextion_language = target;
        sendCommand("page dp");
        // 페이지를 다시 불러오면 pgChipCount 의 칩 수치가 HMI 기본값으로 돌아간다.
        cmd = "pgChipCount.vMaxChip.val=" + (String)(int)my["max_chip"];
        sendCommand(cmd.c_str());
        cmd = "pgChipCount.vSacrificeChip.val=" + (String)(int)my["taken_chip"];
        sendCommand(cmd.c_str());
    }
}

/**
 * @brief 초기 연결 후 Nextion 에서 언어/버전 값을 읽어 초기화
 *        - vLanguage: ESP 내부 저장 후 서버와 동기화
 *        - vVersion: 서버 nextion_version 필드에 전송
 */
void NextionInit()
{
    delay(200);

    int lang = NextionGetNum("get pgSetting.vLanguage.val");
    nextion_language = (lang >= 0) ? lang : 1;  // 읽기 실패시 기본값 KO(1)

    int version = NextionGetNum("get pgSetting.vVersion.val");
    if (version >= 0 && (const char *)my["device_name"])
    {
        has2wifi.Send((String)(const char *)my["device_name"],"nextion_version",String(version));
    }

    SyncLanguage();
}
