# [MADIwrite_Futaba_RS30xTTL](https://github.com/Ninagawa123/MADIwrite_Futaba_RS30xTTL)  
  
"MADIwrite_Futaba_RS30xTTL" はFUTABAのRS303MD, RS304MD, RS306MDなどの  
半二重TTLサーボの設定変更用プログラムです.  
Meridian Board - LITE - に搭載したESP32DevkitCで使える他,  
単体のESP32DevkitCにサーボを接続することでも使用可能です.  
  
マディライトの最大の特徴は強力な通信速度書き換えコマンドです.  
**全ての通信速度を網羅した書き換え** を試みますので, 設定した通信速度が不明となったサーボでも復活させることができます.  
また, メーカー保証外となりますが, 通信速度をメーカー標準の **3倍の691200bps** まで引き上げることができます.  
  
その他, サーボID書き換え, 通信方向の正逆設定, 応答時間の短縮設定などが可能です.  
  
## 対応開発環境  
* PlatformIO  
* ArduinoIDE  
* ESP32DevkitC (送信, 書き換えのみ)  
* ESP32DevkitC + Meridian Board - LITE -　(受信可)  
* ESP32DevkitC + ICS変換基板　(受信可)  
* その他、Arduino系の基板はピンや使用シリアルを変更することで使えると思います.  
  
## 使い方  
（１）ソースコード前半の変数設定に希望の状態を設定します. 詳細については変数欄のコメントをご覧ください.  
（２）後述のピンアサインを参考に, ESP32またはMeridian Board -LITE-とRS30x系サーボ"１個"を接続してください.  
　　　Meridian Board -LITE-にサーボを接続する際は, ICS_Rピンにサーボの黒線が外側になるように接続してください.  
（３）ESP32にプログラムを書き込むと自動的に書き換えがスタートします.  
（４）シリアルモニタを有効にすることで, 作業進捗や結果のインフォメーションを確認できます.  
（５）書き換えが終了すると, サーボが動き始めます.  
 　　 プラス方向60度→マイナス方向120度→プラス方向60度→センターで約２秒停止, という動きを繰り返します.  
  
### ピンアサインは以下の通りです.  
<img width="400" alt="SS 2381" src="https://user-images.githubusercontent.com/8329123/180610583-7db88a6d-a2e5-4185-b453-799409a147b4.png">  
  
**Meridian Board -LITE- 使用時のPin Assign**  (ボードのICS_Rにサーボの黒線がGNDとなるよう接続してください.)  
  RX :[ESP32 pin16 Serial2] - [Meridian Board -LITE- ICS_R] (ICS変換基板 TX)  
  TX :[ESP32 pin17 Serial2] - [Meridian Board -LITE- ICS_R] (ICS変換基板 RX)  
  EN :[ESP32 pin4  Serial2] - [Meridian Board -LITE- ICS_R] (ICS変換基板 EN)  
  5V :[ESP32 5V           ] - [Meridian Board -LITE- 5V   ] (ICS変換基板 IOREF)  
  GND:[ESP32 GND          ] - [Meridian Board -LITE- GND  ] (ICS変換基板 GND)  
  
  
  
<img width="400" alt="SS 2380" src="https://user-images.githubusercontent.com/8329123/180610578-e3c3789b-d0c0-4d4b-a6f0-79706188a41a.png">  
  
**ESP32DevkitC単体 Pin Assign**  
  TX :[ESP32 pin17 Serial2] - [FUTABA RS30x Servo SIGNAL](WHITE/RED&WING)  
  5V :[ESP32 5V           ] - [FUTABA RS30x Servo POWER ](RED)  
  GND:[ESP32 5V           ] - [FUTABA RS30x Servo GND   ](BLACK) 
  
  
### ソースコード中の下記の変数を設定することで実行内容を決めてください.  
------------  
  
| | | |
|-------------|-------------|-------------|
|0x00:9600|0x04:38,400|0x08:153,600 |
|0x01:14,400 |0x05:57,600|0x09:230,400|
|0x02:19,200|0x06:76,800|0x0A:460,800|
|0x03:28.800|0x07:115,200 |0x0B:691,200|
  
// [1] 目標とする通信速度を選んでください. (上図参照)  
unsigned char TARGET_BAUD_RATE = 0x07;  
  
// [2] マディライトしますか？ 0:no 1:yes (全通信速度を網羅する書き込み)  
bool USE_MADIWRITE = 0;  
  
// [3] IDを何番に書き換えますか？　（番号 1~127. 書き換えない場合は0.)  
int NewID = 0;  
  
// [4] サーボの回転方向は？　（0:正転, 時計回りが+となる. 1:逆転. 2以上:設定しない)  
int CW = 2;  
  
// [5] 返信ディレイタイムは？　（0~127. 100μs + 50μs x 数値. 1msなら18　128以上:設定しない)  
int ResDealy = 128;  
  
// [6] ファクトリーリセットしますか？　（0:no 1:yes)  
int AllReset = 0;  
  
------------  
  
