ifwebp.spi	Ver 1.6

・概要
WebP画像を表示するためのsusieプラグイン(32bit)
google公式のlibwebpライブラリをそのまま使っているので大抵のWebPは表示出来るはず
アルファチャンネルについては無視している。
Windows 7 64bit上の あふw でのみ動作確認
libwebpのページ
	https://developers.google.com/speed/webp/

・更新履歴
Ver 1.6 2017-03-20
	libwebp-0.6.0ベースに更新
Ver 1.5	2013-01-15
	libwebp-0.2.0ベースに更新
	出力BITMAPのヘッダーに問題があったのを修正
Ver 1.4	2012-07-24
	losslessのWebPはヘッダーが異なるために非対応と判断され、表示出来なかったのを修正
Ver 1.3 2012-07-20
	libwebp-0.1.99ベースに更新
Ver 1.2 2011-10-14
	libwebp-0.1.3ベースに更新
Ver 1.1 2011-08-16
	特定の画像で必ずエラーが出る問題を修正
		susieプラグインの仕様を勘違いしてた。
Ver 1.0 2011-05-30
	サンプル画像が表示出来ることは確認
