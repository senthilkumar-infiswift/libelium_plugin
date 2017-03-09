<?php
/*
 *  Copyright (C) 2008 Libelium Comunicaciones Distribuidas S.L.
 *  http://www.libelium.com
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *                                                        )[            ....
                                                       -$wj[        _swmQQWC
                                                        -4Qm    ._wmQWWWW!'
                                                         -QWL_swmQQWBVY"~.____
                                                         _dQQWTY+vsawwwgmmQWV!
                                        1isas,       _mgmQQQQQmmQWWQQWVY!"-
                                       .s,. -?ha     -9WDWU?9Qz~- -- -
                                       -""?Ya,."h,   <!`_mT!2-?5a,
                                       -Swa. Yg.-Q,  ~ ^`  /`   "$a.
     aac  <aa, aa/                aac  _a,-4c ]k +m               "1
    .QWk  ]VV( QQf   .      .     QQk  )YT`-C.-? -Y  .
    .QWk       WQmymmgc  <wgmggc. QQk       wgz  = gygmgwagmmgc
    .QWk  jQQ[ WQQQQQQW;jWQQ  QQL QQk  ]WQ[ dQk  ) QF~"WWW(~)QQ[
    .QWk  jQQ[ QQQ  QQQ(mWQ9VVVVT QQk  ]WQ[ mQk  = Q;  jWW  :QQ[
     WWm,,jQQ[ QQQQQWQW')WWa,_aa. $Qm,,]WQ[ dQm,sj Q(  jQW  :QW[
     -TTT(]YT' TTTYUH?^  ~TTB8T!` -TYT[)YT( -?9WTT T'  ]TY  -TY(

                          www.libelium.com

*  Libelium Comunicaciones Distribuidas SL
*  Author: Diego Becerrica, Esteban Gutierrez
*
*/

$cloud_name = "Infiswift Cloud";
$cloud_icon = "images/logo.png"; // Size: 173x84 px. Format: PNG. Transparent background and logo centered.

$CLOUD_LINK = "http://www.infiswift.com";
$CLOUD_FOLDER = "infiswift";
$CLOUD_SECTION = "infiswift"; //section name in config file (setup.ini)
$CLOUD_CONFIG_FILE='/mnt/lib/cfg/infiswift/setup.ini';
$CLOUD_DAEMON_FILE_NAME='CloudInfiswiftD.sh';
$CLOUD_DAEMON_FILE="/etc/init.d/" . $CLOUD_DAEMON_FILE_NAME;
$CLOUD_SH_FILE_NAME='CloudInfiswift.sh';
$CLOUD_TOPIC_FILE='/mnt/lib/cfg/infiswift/topic_template';
$CLOUD_MSG_FILE='/mnt/lib/cfg/infiswift/message_template';
$CLOUD_LOG_FILE='/mnt/user/logs/infiswift.log';
$CLOUD_SYNC_MASK=64;

$CLOUD_PLUGIN = "plugins/". $section. "/". $plugin . "/clouds/".$CLOUD_FOLDER."/";
$CLOUD_SCRIPT_PATH = "../clouds/".$CLOUD_FOLDER."/";
$plugin_main_file="main.php";  // BETER IF USED THE STANDARD main.php
$plugin_server_file="server.php"; // BETER IF USED THE STANDARD server.php

?>
