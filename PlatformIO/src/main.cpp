
// "MADI write for ESP32 RS30x" 2022.07.23 by Izumi Ninagawa
// 参考)https://www.futaba.co.jp/product/robot/robot_download/sample_programs
//     https://d21xsz2za2b5mz.cloudfront.net/6716/1905/0042/RS303MR_RS304MD_118.pdf

// "MADI write for ESP32 RS30x" はFUTABAのRS303MD, RS304MD, RS306MDなどの
// 半二重TTLサーボの設定変更プログラムです.
// Meridian Board - LITE - に搭載したESP32DevkitCで使える他,
// 単体のESP32DevkitCにサーボを接続することでも使用可能です.
//
// マディライトの最大の特徴は強力な通信速度書き換えコマンドです.
// 全ての通信速度を網羅した書き換えを試みますので, 設定した通信速度が不明となったサーボでも復活させることができます.
// また, メーカー保証外となりますが, 通信速度をメーカー標準の3倍の691200bpsまで引き上げることができます。
//
// その他, サーボID書き換え, 通信方向の正逆設定, 応答時間の短縮設定などが可能です.
//
//【使い方】
//（１）ソースコード前半の変数設定に希望の状態を設定します. 詳細については変数欄のコメントをご参考ください.
//（２）後述のピンアサインを参考に、ESP32またはMeridian Board -LITE-とRS30x系サーボ"１個"を接続してください。
//（３）ESP32にプログラムを書き込むと自動的に書き換えがスタートします。
//（４）シリアルモニタを有効にすることで、作業進捗や結果のインフォメーションを確認できます。
//（５）書き換えが終了すると、サーボが動き始めます。
// 　　 プラス方向60度→マイナス方向120度→プラス方向60度→センターで約２秒停止、という動きを繰り返します。
//
// ピンアサインは以下の通りです.
// Meridian Board -LITE- 使用時のPin Assign
//  (ボードのICS_Rにサーボの黒線がGNDとなるよう接続してください。)
// RX :ESP32 pin16 Serial2 :Meridian Board -LITE- ICS_R / ICS変換基板 TX
// TX :ESP32 pin17 Serial2 :Meridian Board -LITE- ICS_R / ICS変換基板 RX
// EN :ESP32 pin4  Serial2 :Meridian Board -LITE- ICS_R / ICS変換基板 EN
// 5V :ESP32 5V            :Meridian Board -LITE- 5V    / ICS変換基板 IOREF
// GND:ESP32 GND           :Meridian Board -LITE- GND   / ICS変換基板 GND
//
// ESP32DevkitC単体 Pin Assign
// TX :ESP32 pin17 Serial2 :FUTABA RS30x Servo SIGNAL(WHITE/RED&WING)
// 5V :ESP32 5V            :FUTABA RS30x Servo POWER (RED)
// GND:ESP32 5V            :FUTABA RS30x Servo GND   (BLACK)
//

// 0x00:9600       0x04:38,400      0x08:153,600
// 0x01:14,400     0x05:57,600      0x09:230,400
// 0x02:19,200     0x06:76,800      0x0A:460,800
// 0x03:28.800     0x07:115,200     0x0B:691,200

// [1] 目標とする通信速度を選んでください.
unsigned char TARGET_BAUD_RATE = 0x0B;

// [2] マディライトしますか？ 0:no 1:yes (全通信速度を網羅する書き込み)
bool USE_MADIWRITE = 0;

// [3] IDを何番に書き換えますか？　（番号 1~127. 書き換えない場合は0.)
int NewID = 0;

// [4] サーボの回転方向設定は？　（0:正転, 時計回りが+となる. 1:逆転. 2以上:設定しない)
int CW = 2;

// [5] 返信ディレイタイムは？　（0~127. 100μs + 50μs x 数値. 1msなら18　128以上:設定しない)
int ResDealy = 128;

// [6] ファクトリーリセットしますか？　（0:no 1:yes)
int AllReset = 0;

// [7] 使用環境は？　（0:ESP32DevkitCのみ 1:ESP32+Meridian Board -LITE- or ICS変換基板）
int Device = 0; // 1 の場合は返信をシリアルモニタで表示します.

// ******************************** 設定はここまで ************************************

/* グローバル変数定義 */
int EN_R_PIN = 4;     // デジタルPin23を送信イネーブルピンに設定
int RS30x_speed = 50; //サーボ速度指定
int FutabaBaudRates[12] = {9600, 14400, 19200, 28800, 38400, 57600, 76800, 115200, 153600, 230400, 460800, 691200};
#include <Arduino.h>

////////////　ボ ー レ ー ト  の 表 示 用 変 換　////////////
int BaudRateDisp(unsigned char dat)
{
    int tmp = FutabaBaudRates[(int)dat];
    return tmp;
}

/////////////////　パ ケ ッ ト 送 信　/////////////////////
void SendPacket(unsigned char *allay, int len)
{
    digitalWrite(EN_R_PIN, HIGH); // 送信許可
    for (int i = 0; i < len; i++)
    {
        Serial2.write(allay[i]);
    }
    Serial2.flush();             // データ送信完了待ち
    digitalWrite(EN_R_PIN, LOW); // 送信禁止
}

//////////////// RS30x R O M 書 き 込 み ////////////////
void RS30x_RomWrite(unsigned char ID)
{
    unsigned char RS30x_s_data[8];   // 送信データバッファ [9byte]
    unsigned char RS30x_s_cksum = 0; // チェックサム計算用変数

    // ROM書き込みパケットデータ生成
    RS30x_s_data[0] = 0xFA; // Header
    RS30x_s_data[1] = 0xAF; // Header
    RS30x_s_data[2] = ID;   // ID
    RS30x_s_data[3] = 0x40; // Flags
    RS30x_s_data[4] = 0xFF; // Address
    RS30x_s_data[5] = 0x00; // Length
    RS30x_s_data[6] = 0x00; // Count

    // チェックサム計算
    RS30x_s_cksum = 0;
    for (int i = 2; i <= 6; i++)
    {
        RS30x_s_cksum = RS30x_s_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    // Serial.println();
    RS30x_s_data[7] = RS30x_s_cksum; // Sum

    SendPacket(RS30x_s_data, 8); // パケットデータ送信

    delay(100);
}

////////////////// RS30x リ ブ ー ト ///////////////////
void RS30x_Reboot(unsigned char ID)
{
    unsigned char RS30x_s_data[8];   // 送信データバッファ [9byte]
    unsigned char RS30x_s_cksum = 0; // チェックサム計算用変数

    // 再起動書き込みパケットデータ生成
    RS30x_s_data[0] = 0xFA; // Header
    RS30x_s_data[1] = 0xAF; // Header
    RS30x_s_data[2] = ID;   // ID
    RS30x_s_data[3] = 0x20; // Flags
    RS30x_s_data[4] = 0xFF; // Address
    RS30x_s_data[5] = 0x00; // Length
    RS30x_s_data[6] = 0x00; // Count

    // チェックサム計算
    RS30x_s_cksum = 0;
    for (int i = 2; i <= 6; i++)
    {
        RS30x_s_cksum = RS30x_s_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    // Serial.println();
    RS30x_s_data[7] = RS30x_s_cksum; // Sum

    SendPacket(RS30x_s_data, 8); // パケットデータ送信
    delay(100);                  // ROM書込み時間待機。
}

////////////　書き込みとリブート　////////////
void Write_and_Reboot()
{
    RS30x_RomWrite(255); //※※※※何らかの設定を変更する際はこの行をコメントアウト※※※※
    delay(300);
    RS30x_Reboot(255); //※※※※何らかの設定を変更する際はこの行をコメントアウト※※※※
    delay(600);
}

///////////////　角　度　デ　ー　タ　取　得　///////////////
void ReadAngle(unsigned char ID) // 引数はサーボID
{
    unsigned char RS30x_s_data[8]; // 送信データバッファ [8byte]
    unsigned char CheckSum = 0;    // チェックサム計算用変数

    // パケットデータ生成
    RS30x_s_data[0] = 0xFA; // Header
    RS30x_s_data[1] = 0xAF; // Header
    RS30x_s_data[2] = ID;   // ID
    RS30x_s_data[3] = 0x0F; // Flags
    RS30x_s_data[4] = 0x2A; // Address
    RS30x_s_data[5] = 0x02; // Length
    RS30x_s_data[6] = 0x00; // Count
    // チェックサム計算
    for (int i = 2; i <= 6; i++)
    {
        CheckSum = CheckSum ^ RS30x_s_data[i]; // ID～DATAまでのXOR
    }
    RS30x_s_data[7] = CheckSum; // Sum

    SendPacket(RS30x_s_data, 8); // パケットデータ送信
}

///////////////　角　度　デ　ー　タ　受 信　///////////////
int WaitReadAngle(void)
{
    unsigned char RxData[8];    // 送信データバッファ [8byte]
    unsigned char CheckSum = 0; // チェックサム計算用変数
    int AngleData = 0;          // 角度データ

    // リターンパケット待ち
    while (Serial2.read() != 0xFD)
    {
    } // Header
    while (Serial2.read() != 0xDF)
    {
    } // Header
    while (Serial2.available() < 8)
    {
    } // ID～Sem

    for (int i = 0; i < 7; i++)
    {
        RxData[i] = Serial2.read();
        CheckSum = CheckSum ^ RxData[i]; // ID～DATAまでのXOR
    }

    // チェックサムが一致すれば、角度データ読み出し
    if (CheckSum == Serial2.read())
    {
        AngleData = (int)RxData[6]; // Hi byte
        AngleData = AngleData << 8;
        AngleData |= (int)RxData[5]; // Lo byte
    }

    return AngleData;
}

///////////////　RS30x サ ー ボ ID 設 定　///////////////
void RS30x_NewID(unsigned char dat)
{
    unsigned char RS30x_s_data[9];   // 送信データバッファ [9byte]
    unsigned char RS30x_s_cksum = 0; // チェックサム計算用変数

    // パケットデータ生成
    RS30x_s_data[0] = 0xFA; // Header
    RS30x_s_data[1] = 0xAF; // Header
    RS30x_s_data[2] = 0xFF; // ID(全サーボ対象)
    RS30x_s_data[3] = 0x00; // Flags
    RS30x_s_data[4] = 0x04; // Address
    RS30x_s_data[5] = 0x01; // Length
    RS30x_s_data[6] = 0x01; // Count
    RS30x_s_data[7] = dat;  // dat

    // チェックサム計算
    for (int i = 2; i < 8; i++)
    {
        RS30x_s_cksum = RS30x_s_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    RS30x_s_data[8] = RS30x_s_cksum; // Sum

    SendPacket(RS30x_s_data, 9); // パケットデータ送信

    delay(1000);
    Serial.print("New Servo ID is ");
    Serial.println(dat);
    delay(10);
}

//////////RS30x 返 信 デ ィ レ イ 時 間 の 設 定///////////
void RS30x_SetReplayDelay(unsigned char dat)
{
    unsigned char RS30x_s_data[9];   // 送信データバッファ [9byte]
    unsigned char RS30x_s_cksum = 0; // チェックサム計算用変数

    // 再起動書き込みパケットデータ生成
    RS30x_s_data[0] = 0xFA; // Header
    RS30x_s_data[1] = 0xAF; // Header
    RS30x_s_data[2] = 0xFF; // 全サーボ対象
    RS30x_s_data[3] = 0x00; // Flags
    RS30x_s_data[4] = 0x07; // Address
    RS30x_s_data[5] = 0x01; // Length
    RS30x_s_data[6] = 0x01; // Count
    RS30x_s_data[7] = dat;  // dat

    // チェックサム計算
    for (int i = 2; i < 8; i++)
    {
        RS30x_s_cksum = RS30x_s_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    RS30x_s_data[8] = RS30x_s_cksum; // checksum

    SendPacket(RS30x_s_data, 9); // パケットデータ送信

    Serial.print("Response Delay Time set to ");
    Serial.print(short(dat) * 50 + 100);
    Serial.println(" μs.");
    delay(500);
}

//////////// RS30x サ ー ボ リ バ ー ス 設 定 ////////////
void RS30x_Reverse(unsigned char dat)
{                                    // dat 0x00=Nomal 0x01=Reverse
    unsigned char RS30x_s_data[9];   // 送信データバッファ [9byte]
    unsigned char RS30x_s_cksum = 0; // チェックサム計算用変数

    // パケットデータ生成
    RS30x_s_data[0] = 0xFA; // Header
    RS30x_s_data[1] = 0xAF; // Header
    RS30x_s_data[2] = 0xFF; // ID
    RS30x_s_data[3] = 0x00; // Flags
    RS30x_s_data[4] = 0x05; // Address
    RS30x_s_data[5] = 0x01; // Length
    RS30x_s_data[6] = 0x01; // Count
    RS30x_s_data[7] = dat;  // dat

    // チェックサム計算
    for (int i = 2; i < 8; i++)
    {
        RS30x_s_cksum = RS30x_s_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    RS30x_s_data[8] = RS30x_s_cksum; // Sum

    SendPacket(RS30x_s_data, 9); // パケットデータ送信

    if (dat == 0)
    {
        Serial.println("Rotatin changed to FORWARD, CW.");
    }
    else if (dat == 1)
    {
        Serial.println("Rotatin changed to REWARD, CCW.");
    }
    delay(500);
}

////////////　RS30x サ ー ボ ト ル ク 設 定　/////////////
void RS30x_Torque(unsigned char ID, unsigned char dat)
{
    unsigned char RS30x_s_data[9];   // 送信データバッファ [9byte]
    unsigned char RS30x_s_cksum = 0; // チェックサム計算用変数

    // パケットデータ生成
    RS30x_s_data[0] = 0xFA; // Header
    RS30x_s_data[1] = 0xAF; // Header
    RS30x_s_data[2] = ID;   // ID
    RS30x_s_data[3] = 0x00; // Flags
    RS30x_s_data[4] = 0x24; // Address
    RS30x_s_data[5] = 0x01; // Length
    RS30x_s_data[6] = 0x01; // Count
    RS30x_s_data[7] = dat;  // dat

    // チェックサム計算
    for (int i = 2; i < 8; i++)
    {
        RS30x_s_cksum = RS30x_s_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    RS30x_s_data[8] = RS30x_s_cksum; // Sum

    SendPacket(RS30x_s_data, 9); // パケットデータ送信
}

////////// RS30x サ ー ボ 角 度 ・ 速 度 指 定 ///////////
void RS30x_Move(unsigned char ID, int Angle, int Speed)
{
    unsigned char RS30x_s_data[12];  // 送信データバッファ [12byte]
    unsigned char RS30x_s_cksum = 0; // チェックサム計算用変数

    // パケットデータ生成
    RS30x_s_data[0] = 0xFA; // Header
    RS30x_s_data[1] = 0xAF; // Header
    RS30x_s_data[2] = ID;   // ID
    RS30x_s_data[3] = 0x00; // Flags
    RS30x_s_data[4] = 0x1E; // Address
    RS30x_s_data[5] = 0x04; // Length
    RS30x_s_data[6] = 0x01; // Count
    // Angle
    RS30x_s_data[7] = (unsigned char)0x00FF & Angle;        // Low byte
    RS30x_s_data[8] = (unsigned char)0x00FF & (Angle >> 8); // Hi  byte
    // Speed
    RS30x_s_data[9] = (unsigned char)0x00FF & Speed;         // Low byte
    RS30x_s_data[10] = (unsigned char)0x00FF & (Speed >> 8); // Hi  byte
    // チェックサム計算
    for (int i = 2; i < 11; i++)
    {
        RS30x_s_cksum = RS30x_s_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    RS30x_s_data[11] = RS30x_s_cksum; // Sum

    SendPacket(RS30x_s_data, 12); // パケットデータ送信
}

/////////////　フ ァ ク ト リ ー リ セ ッ ト　/////////////
void FactoryReset()
{
    unsigned char RS30x_s_data[8];   // 送信データバッファ [8byte]
    unsigned char RS30x_s_cksum = 0; // チェックサム計算用変数

    // パケットデータ生成
    RS30x_s_data[0] = 0xFA; // Header
    RS30x_s_data[1] = 0xAF; // Header
    RS30x_s_data[2] = 0xFF; // ID(全サーボ対象)
    RS30x_s_data[3] = 0x10; // Flags
    RS30x_s_data[4] = 0xFF; // Address
    RS30x_s_data[5] = 0xFF; // Length
    RS30x_s_data[6] = 0x00; // Count

    // チェックサム計算
    for (int i = 2; i < 7; i++)
    {
        RS30x_s_cksum = RS30x_s_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    RS30x_s_data[7] = RS30x_s_cksum; // Sum
    SendPacket(RS30x_s_data, 8);     // パケットデータ送信
    delay(1000);
    Write_and_Reboot();
}

////////////////// マ デ ィ ラ イ ト ///////////////////
void RS30x_SetSerialSpeedAll()
{
    unsigned char RS30x_s_data[9];   // 送信データバッファ [9byte]
    unsigned char RS30x_s_cksum = 0; // チェックサム計算用変数

    // パケットデータ生成
    RS30x_s_data[0] = 0xFA;             // Header
    RS30x_s_data[1] = 0xAF;             // Header
    RS30x_s_data[2] = 0xFF;             // ID
    RS30x_s_data[3] = 0x00;             // Flags
    RS30x_s_data[4] = 0x06;             // Address
    RS30x_s_data[5] = 0x01;             // Length
    RS30x_s_data[6] = 0x01;             // Count
    RS30x_s_data[7] = TARGET_BAUD_RATE; // dat

    Serial.print("MADI Write RS30x servo's BAUDRATE to ");
    Serial.println(FutabaBaudRates[int(TARGET_BAUD_RATE)]);

    // チェックサム計算
    for (int i = 2; i < 8; i++)
    {
        RS30x_s_cksum = RS30x_s_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    RS30x_s_data[8] = RS30x_s_cksum; // Sum

    Serial.print("Processing...");

    for (int i = 0; i < 12; i++)
    {
        // Serial.println("near Serial2.begin(FutabaBaudRates[i]).");

        Serial2.begin(FutabaBaudRates[i]); // 現在のシリアルサーボのボーレート（デフォルトは115,200bps）
        // Serial2.begin(FutabaBaudRates[i]); // 現在のシリアルサーボのボーレート（デフォルトは115,200bps）

        SendPacket(RS30x_s_data, 9); // パケットデータ送信

        Write_and_Reboot();

        Serial.print(11 - i);
        if (i < 11)
        {
            Serial.print(",");
        }
        else
        {
            Serial.print(".");
        }
        delay(100);
        Serial2.end();
    }
    Serial.println();
}

// ■ サ ー ボ 角 度 の 読 み 込 み _R -------------------------------
// サーボ角度データ呼び出し　RS30x_CallAngle_R ([Servo ID])
// 返り値は角度degree*10
float RS30x_ReadAngle_R(unsigned char ID)
{
    unsigned char RS30x_s_data[8]; // 送信データバッファ [8byte]
    unsigned char RS30x_r_data[8]; // 受信データバッファ [8byte]
    unsigned char RS30x_cksum = 0; // チェックサム計算用変数
    int Angledat = 0;              // 角度データ
    float result = 0;              // 角度データ

    // パケットデータ生成
    RS30x_s_data[0] = 0xFA; // Header
    RS30x_s_data[1] = 0xAF; // Header
    RS30x_s_data[2] = ID;   // ID
    RS30x_s_data[3] = 0x0F; // Flags
    RS30x_s_data[4] = 0x2A; // Address
    RS30x_s_data[5] = 0x02; // Length
    RS30x_s_data[6] = 0x00; // Count

    // チェックサム計算
    for (int i = 2; i <= 6; i++)
    {
        RS30x_cksum = RS30x_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    RS30x_s_data[7] = RS30x_cksum; // Sum

    SendPacket(RS30x_s_data, 8); // パケットデータ送信

    // リターンパケット待ち
    while (Serial2.read() != 0xFD)
    {
    } // Header
    while (Serial2.read() != 0xDF)
    {
    } // Header
    while (Serial2.available() < 8)
    {
    } // ID～Sem

    // チェックサム計算
    RS30x_cksum = 0; //チェックサムリセット
    for (int i = 0; i <= 6; i++)
    {
        RS30x_r_data[i] = Serial2.read();
        RS30x_cksum = RS30x_cksum ^ RS30x_r_data[i]; // ID～datまでのXOR
    }

    // チェックサムが一致すれば、角度データ読み出し
    if (RS30x_cksum == Serial2.read())
    {
        Angledat = (int)RS30x_r_data[6]; // Hi byte
        Angledat = Angledat << 8;
        Angledat |= (int)RS30x_r_data[5]; // Lo byte
    }
    result = float(short(Angledat));

    return result;
}

// デ ー タ ま と め て 読 み 込 み 表 示 -------------------------------
void RS30x_Read_Data(unsigned char id, unsigned char add, unsigned char len)
{
    int ReturnLen = 6 + len;
    unsigned char RS30x_s_data[8];         // 送信データバッファ [8byte]
    unsigned char RS30x_r_data[ReturnLen]; // 受信データバッファ [8byte]
    unsigned char RS30x_cksum = 0;         // チェックサム計算用変数

    // パケットデータ生成
    RS30x_s_data[0] = 0xFA; // Header
    RS30x_s_data[1] = 0xAF; // Header
    RS30x_s_data[2] = id;   // ALL ID
    RS30x_s_data[3] = 0x0F; // Flags
    RS30x_s_data[4] = add;  // Address
    RS30x_s_data[5] = len;  // Length
    RS30x_s_data[6] = 0x00; // Count

    // チェックサム計算
    for (int i = 2; i <= 6; i++)
    {
        RS30x_cksum = RS30x_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    RS30x_s_data[7] = RS30x_cksum; // Sum

    SendPacket(RS30x_s_data, 8); // パケットデータ送信

    // リターンパケット待ち
    while (Serial2.read() != 0xFD)
    {
    } // Header
    while (Serial2.read() != 0xDF)
    {
    } // Header
    while (Serial2.available() < ReturnLen)
    {
    } // ID～Sem

    // チェックサム計算
    RS30x_cksum = 0; //チェックサムリセット
    for (int i = 0; i < ReturnLen - 1; i++)
    {
        RS30x_r_data[i] = Serial2.read();
        // Serial.print(RS30x_r_data[i], HEX);
        // Serial.print(" ");
        RS30x_cksum = RS30x_cksum ^ RS30x_r_data[i]; // ID～datまでのXOR
    }

    unsigned char IDdatraw[ReturnLen]; // Lo byte

    // チェックサムが一致すれば、IDデータ読み出し
    if (RS30x_cksum == Serial2.read())
    {

        // Serial.println("Check OK.");
        for (int i = 0; i < ReturnLen - 1; i++)
        {

            // IDdat = (int)RS30x_r_dat[i + 1]; // Hi byte
            // IDdat = IDdat << 8;
            // IDdat |= (int)RS30x_r_dat[i]; // Lo byte

            IDdatraw[i * 2] = (int)RS30x_r_data[i * 2]; // Lo byte
            // Serial.print(IDdatraw[i * 2], HEX);
            // Serial.print(" ");

            IDdatraw[i * 2 + 1] = (int)RS30x_r_data[i * 2 + 1]; // Lo byte
            // Serial.print(IDdatraw[i * 2 + 1], HEX);
            // Serial.print(" ");
        }
    }
    Serial.print("         ID : ");
    Serial.println(IDdatraw[5]);
    Serial.print("   Rotation : ");
    if (IDdatraw[6] == 0)
    {
        Serial.println("Foward, CW");
    }
    else
    {
        Serial.println("Reward, CCW");
    }
    Serial.print("      Speed : ");
    Serial.print(BaudRateDisp(IDdatraw[7]));
    Serial.println(" bps");

    Serial.print("ReturnDelay : ");
    Serial.print(short(IDdatraw[8]) * 50 + 100);
    Serial.println(" μs");
}

void setup()
{
    pinMode(EN_R_PIN, OUTPUT); // デジタルPin2(EN_R_PIN)を出力に設定
    Serial.begin(200000);      // Teensy4.0とPCとのシリアル通信速度
    delay(150);
    Serial.println();

    if (USE_MADIWRITE) // マディライトをするかどうか
    {
        RS30x_SetSerialSpeedAll(); //全帯域対象のボーレート変更
    }
    Serial2.begin(FutabaBaudRates[int(TARGET_BAUD_RATE)]); // 現在のシリアルサーボのボーレート（デフォルトは115,200bps）

    Serial.print("Now Serial begin in "); // 現在の通信速度のボーレートを表示
    Serial.print(BaudRateDisp(TARGET_BAUD_RATE));
    Serial.println(" bps.");

    if (AllReset == 1) // ファクトリーリセットをするか
    {
        delay(1000);
        Serial.print("Execute");
        delay(1000);
        Serial.print(" Factory");
        delay(1000);
        Serial.println(" Reset.");
        delay(1000);

        FactoryReset();
        Write_and_Reboot();
        Serial2.flush();
        Serial2.end();
        delay(100);
        TARGET_BAUD_RATE = 0x07;
        Serial2.begin(FutabaBaudRates[int(TARGET_BAUD_RATE)]); // 現在のシリアルサーボのボーレート（デフォルトは115,200bps）
        Serial.print("Now Serial restarted in ");              // 現在の通信速度のボーレートを表示
        Serial.print(BaudRateDisp(TARGET_BAUD_RATE));
        Serial.println(" bps.");
    }

    if (NewID != 0) // IDを書き込むかどうか
    {
        RS30x_NewID((unsigned char)NewID);
        Write_and_Reboot();
    }

    if (ResDealy < 128) // リターンディレイの設定
    {
        RS30x_SetReplayDelay((unsigned char)ResDealy);
        Write_and_Reboot();
    }

    if (CW < 2) // 回転方向の設定
    {
        RS30x_Reverse((unsigned char)CW);
        Write_and_Reboot();
    }

    Serial.println();
    Serial.println("Servo Information from RS30x..."); // サーボから受信するデータの表示

    // 全サーボトルクオン
    RS30x_Torque(255, 0x01); // ID = 1(0x01) , RS30x_Torque = ON   (0x01)
    delay(1000);

    if (Device == 1) // 半二重回路がある場合にはシリアルモニタにサーボ情報を表示
    {
        RS30x_Read_Data(0xFF, 0x04, 0x04); // ID, Addres, Length 接続されたサーボの情報を表示
    }
}

void loop()
{
    RS30x_Move(255, 0, RS30x_speed); // ID=255は全サーボ , GoalPosition = 0deg(100) , Time = 1.0sec(RS30x_speed=100)
    Serial.print("+,");
    delay(1000);
    Serial.print("0,");
    delay(1000);
    Serial.print("0,");
    delay(1000);
    RS30x_Move(255, 600, RS30x_speed); // ID=255は全サーボ , GoalPosition = 10.0deg(100) , Time = 1.0sec(RS30x_speed=100)
    Serial.print("+,");
    delay(1000);
    RS30x_Move(255, -600, RS30x_speed); // ID=255は全サーボ , GoalPosition = -10.0deg(-100) , Time = 1.0sec(RS30x_speed=100)
    Serial.print("-,");
    delay(1000);
}
