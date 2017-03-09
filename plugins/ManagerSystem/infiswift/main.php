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
*  Author: Joaquín Ruiz, Esteban Gutierrez
*
*/

    global $section, $plugin, $cloud;

    include_once $CLOUD_PLUGIN.'php/functions.php';
    $_plugin_javascript[] =  $CLOUD_SCRIPT_PATH."javascript/scripts.js";

    //Check that the synchronization service is running
    $MQTT_running = exec ("sudo ps aux | grep $CLOUD_SH_FILE_NAME | grep -v grep|wc -l");

    $html.= '
        <div id="plugin_content">
            <a href="'.$CLOUD_LINK.'" class="cloud_link" target="_blank" title="'.$cloud_name.'">
                <img src="'.$CLOUD_PLUGIN. $cloud_icon .'" width="173" height="84"/>
            </a>

            <div class="form">
                <b>Configuration</b>
                <hr />
                <form id="setup">
                    <table id="MQTT">'.
                        get_cloud_config() //Get configuration values
                    .'</table>
                </form>
            </div>

            <div class="gap"></div>

            <div class="form">
                <div id="daemonStatus" class="service_status '. (($MQTT_running < 1) ? 'stopped' : 'running') .'">
                    <span class="status"></span>
                    <b>Infiswift MQTT Sender</b>
                    <button type="button" class="start_button" onclick="start_cloud(\''.$section.'\',\''.$plugin.'\',\''.$cloud.'\');"><i class="fa fa-play"></i>  Start</button>
                    <button type="button" class="stop_button" onclick="stop_cloud(\''.$section.'\',\''.$plugin.'\',\''.$cloud.'\');"><i class="fa fa-stop"></i>  Stop</button>
                    <p class="loading"><i class="fa fa-spinner fa-pulse"></i> loading...</p>
                </div>
            </div>
        </div>';



?>
