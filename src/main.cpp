
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
bool USE_MADIWRITE = 1;

// [3] IDを何番に書き換えますか？　（番号 1~127. 書き換えない場合は0.)
int NewID = 1;

// [4] サーボの回転方向は？　（0:正転, 時計回りが+となる. 1:逆転. 2以上:設定しない)
int CW = 0;

// [5] 返信ディレイタイムは？　（1~127. 100μs + 50μs x 数値. 1msなら18　128以上:設定しない)
int ResDealy = 0;

// [6] ファクトリーリセットしますか？　（0:no 1:yes)
int AllReset = 0;

// ************** 設定はここまで ******************

/* グローバル変数定義 */
int EN_R_PIN = 4;     // デジタルPin23を送信イネーブルピンに設定
int RS30x_speed = 50; //サーボ速度指定
int FutabaBaudRates[12] = {9600, 14400, 19200, 28800, 38400, 57600, 76800, 115200, 153600, 230400, 460800, 691200};
#include <Arduino.h>

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
    for (int i = 2; i <= 7; i++)
    {
        RS30x_s_cksum = RS30x_s_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    RS30x_s_data[8] = RS30x_s_cksum; // Sum

    // パケットデータ送信
    digitalWrite(EN_R_PIN, HIGH); // 送信許可
    for (int i = 0; i <= 8; i++)
    {
        Serial2.write(RS30x_s_data[i]);
    }
    Serial2.flush();             // データ送信完了待ち
    digitalWrite(EN_R_PIN, LOW); // 送信禁止

    Serial.print("New Servo ID is ");
    Serial.println(dat);
}

//////////RS30x 返 信 デ ィ レ イ 時 間 の 設 定///////////
void RS30x_SetReplayDelay(unsigned char dat)
{
    unsigned char RS30x_s_data[8];   // 送信データバッファ [9byte]
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
    for (int i = 2; i <= 7; i++)
    {
        RS30x_s_cksum = RS30x_s_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    RS30x_s_data[8] = RS30x_s_cksum; // Sum

    // パケットデータ送信
    digitalWrite(EN_R_PIN, HIGH); // 送信許可
    for (int i = 0; i < 8; i++)
    {
        Serial2.write(RS30x_s_data[i]);
    }
    Serial2.flush();             // データ送信完了待ち
    digitalWrite(EN_R_PIN, LOW); // 送信禁止

    Serial.print("Response Delay Time set to ");
    Serial.print(short(dat) * 0.05 + 0.1, 2);
    Serial.println(" ms.");
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
    for (int i = 2; i <= 7; i++)
    {
        RS30x_s_cksum = RS30x_s_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    RS30x_s_data[8] = RS30x_s_cksum; // Sum

    // パケットデータ送信
    digitalWrite(EN_R_PIN, HIGH); // 送信許可
    for (int i = 0; i <= 8; i++)
    {
        Serial2.write(RS30x_s_data[i]);
    }
    Serial2.flush();             // データ送信完了待ち
    digitalWrite(EN_R_PIN, LOW); // 送信禁止

    if (dat == 0)
    {
        Serial.println("Rotatin changed to FORWARD, CW.");
    }
    else if (dat == 1)
    {
        Serial.println("Rotatin changed to REWARD, CCW.");
    }
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
    for (int i = 2; i <= 7; i++)
    {
        RS30x_s_cksum = RS30x_s_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    RS30x_s_data[8] = RS30x_s_cksum; // Sum

    // パケットデータ送信
    digitalWrite(EN_R_PIN, HIGH); // 送信許可
    for (int i = 0; i <= 8; i++)
    {
        Serial2.write(RS30x_s_data[i]);
    }
    Serial2.flush();             // データ送信完了待ち
    digitalWrite(EN_R_PIN, LOW); // 送信禁止
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
    for (int i = 2; i <= 10; i++)
    {
        RS30x_s_cksum = RS30x_s_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    RS30x_s_data[11] = RS30x_s_cksum; // Sum

    // パケットデータ送信
    digitalWrite(EN_R_PIN, HIGH); // 送信許可
    for (int i = 0; i <= 11; i++)
    {
        Serial2.write(RS30x_s_data[i]);
    }
    Serial2.flush();             // データ送信完了待ち
    digitalWrite(EN_R_PIN, LOW); // 送信禁止
}

/////////////　フ ァ ク ト リ ー リ セ ッ ト　/////////////
void FactoryReset()
{
    unsigned char RS30x_s_data[9];   // 送信データバッファ [9byte]
    unsigned char RS30x_s_cksum = 0; // チェックサム計算用変数

    // パケットデータ生成
    RS30x_s_data[0] = 0xFA; // Header
    RS30x_s_data[1] = 0xAF; // Header
    RS30x_s_data[2] = 0xFF; // ID(全サーボ対象)
    RS30x_s_data[3] = 0x10; // Flags
    RS30x_s_data[4] = 0xFF; // Address
    RS30x_s_data[5] = 0xFF; // Length
    RS30x_s_data[6] = 0x00; // Count
    RS30x_s_data[7] = 0xFF; // dat

    // チェックサム計算
    for (int i = 2; i <= 7; i++)
    {
        RS30x_s_cksum = RS30x_s_cksum ^ RS30x_s_data[i]; // ID～datまでのXOR
    }
    RS30x_s_data[8] = RS30x_s_cksum; // Sum

    // パケットデータ送信
    digitalWrite(EN_R_PIN, HIGH); // 送信許可
    for (int i = 0; i <= 8; i++)
    {
        Serial2.write(RS30x_s_data[i]);
    }
    Serial2.flush();             // データ送信完了待ち
    digitalWrite(EN_R_PIN, LOW); // 送信禁止

    Serial.println("Executed Factory Reset.");
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

    // パケットデータ送信
    digitalWrite(EN_R_PIN, HIGH); // 送信許可
    for (int i = 0; i <= 7; i++)
    {
        Serial2.write(RS30x_s_data[i]);
    }
    delay(100);                  // ROM書込み時間待機。
    Serial2.flush();             // データ送信完了待ち
    digitalWrite(EN_R_PIN, LOW); // 送信禁止
    delay(100);

    // Serial.print("Write data to ROM to ");
    if (ID == 255)
    {
        // Serial.print("ALL Connected");
    }
    else
    {
        // Serial.print(ID, DEC);
    }
    // Serial.println(" RS30x servo's.");
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

    // パケットデータ送信
    digitalWrite(EN_R_PIN, HIGH); // 送信許可
    for (int i = 0; i <= 7; i++)
    {
        Serial2.write(RS30x_s_data[i]);
    }
    Serial2.flush();             // データ送信完了待ち
    digitalWrite(EN_R_PIN, LOW); // 送信禁止

    // Serial.print("Reboot ");
    if (ID == 255)
    {
        // Serial.print("ALL Connected");
    }
    else
    {
        // Serial.print(ID, DEC);
    }
    // Serial.println(" RS30x servo's.");
    delay(100); // ROM書込み時間待機。
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

    String tmp = "";
    switch (TARGET_BAUD_RATE)
    {
    case 0x00:
        tmp = "9600";
        break;
    case 0x01:
        tmp = "14,400";
        break;
    case 0x02:
        tmp = "19,200";
        break;
    case 0x03:
        tmp = "28.800";
        break;
    case 0x04:
        tmp = "38,400";
        break;
    case 0x05:
        tmp = "57,600";
        break;
    case 0x06:
        tmp = "76,800";
        break;
    case 0x07:
        tmp = "115,200";
        break;
    case 0x08:
        tmp = "153,600";
        break;
    case 0x09:
        tmp = "230,400";
        break;
    case 0x0A:
        tmp = "460,800";
        break;
    case 0x0B:
        tmp = "691,200";
        break;
    default:
        tmp = "115,200";
    }

    Serial.print("MADI Write RS30x servo's BAUDRATE to ");
    Serial.println(tmp);

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

        // パケットデータ送信
        digitalWrite(EN_R_PIN, HIGH); // 送信許可
        for (int j = 0; j <= 8; j++)
        {
            Serial2.write(RS30x_s_data[j]);
        }
        // Serial.println("near Serial2.flush().");

        delay(300);
        RS30x_RomWrite(255); //※※※※何らかの設定を変更する際はこの行をコメントアウト※※※※
        delay(500);
        RS30x_Reboot(255); //※※※※何らかの設定を変更する際はこの行をコメントアウト※※※※
        delay(500);

        Serial2.flush();             // データ送信完了待ち
        digitalWrite(EN_R_PIN, LOW); // 送信禁止

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

void setup()
{
    pinMode(EN_R_PIN, OUTPUT); // デジタルPin2(EN_R_PIN)を出力に設定

    Serial.begin(200000); // Teensy4.0とPCとのシリアル通信速度
    delay(1000);
    Serial.println();

    if (USE_MADIWRITE)
    {
        RS30x_SetSerialSpeedAll(); //全帯域対象のボーレート変更
    }
    Serial2.begin(FutabaBaudRates[int(TARGET_BAUD_RATE)]); // 現在のシリアルサーボのボーレート（デフォルトは115,200bps）

    String tmp = "";
    switch (TARGET_BAUD_RATE)
    {
    case 0x00:
        tmp = "9600";
        break;
    case 0x01:
        tmp = "14,400";
        break;
    case 0x02:
        tmp = "19,200";
        break;
    case 0x03:
        tmp = "28.800";
        break;
    case 0x04:
        tmp = "38,400";
        break;
    case 0x05:
        tmp = "57,600";
        break;
    case 0x06:
        tmp = "76,800";
        break;
    case 0x07:
        tmp = "115,200";
        break;
    case 0x08:
        tmp = "153,600";
        break;
    case 0x09:
        tmp = "230,400";
        break;
    case 0x0A:
        tmp = "460,800";
        break;
    case 0x0B:
        tmp = "691,200";
        break;
    default:
        tmp = "115,200";
    }
    Serial.print("Now Serial begin in ");
    Serial.print(tmp);
    Serial.println(" bps.");

    if (AllReset == 1)
    {
        FactoryReset();
        delay(500);
        RS30x_RomWrite(255); //※※※※何らかの設定を変更する際はこの行をコメントアウト※※※※
        delay(1000);
        RS30x_Reboot(255); //※※※※何らかの設定を変更する際はこの行をコメントアウト※※※※
        delay(1000);
    }

    if (NewID != 0)
    {
        RS30x_NewID((unsigned char)NewID);
        delay(500);
        RS30x_RomWrite(255); //※※※※何らかの設定を変更する際はこの行をコメントアウト※※※※
        delay(1000);
        RS30x_Reboot(255); //※※※※何らかの設定を変更する際はこの行をコメントアウト※※※※
        delay(1000);
    }

    if (ResDealy < 128)
    {
        RS30x_SetReplayDelay((unsigned char)ResDealy);
        delay(500);
        RS30x_RomWrite(255); //※※※※何らかの設定を変更する際はこの行をコメントアウト※※※※
        delay(1000);
        RS30x_Reboot(255); //※※※※何らかの設定を変更する際はこの行をコメントアウト※※※※
        delay(1000);
    }
    RS30x_RomWrite(255); // 書き込み
    delay(500);

    if (CW < 2)
    {
        RS30x_Reverse((unsigned char)CW);
        delay(500);
        RS30x_RomWrite(255); //※※※※何らかの設定を変更する際はこの行をコメントアウト※※※※
        delay(1000);
        RS30x_Reboot(255); //※※※※何らかの設定を変更する際はこの行をコメントアウト※※※※
        delay(1000);
    }
    RS30x_RomWrite(255); // 書き込み
    delay(1000);
    delay(1000);
    RS30x_Reboot(255); // リブート
    delay(1000);

    //■ 全サーボトルクオン
    RS30x_Torque(255, 0x01); // ID = 1(0x01) , RS30x_Torque = ON   (0x01)
    delay(1000);
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
