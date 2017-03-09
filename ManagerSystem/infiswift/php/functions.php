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
*  Author: Carlos Arilla, Diego Becerrica, Esteban Gutierrez
*
*/

require_once($API_core.'/Config/Lite.php');
include_once $base_plugin . '/configuration.php';

function start_cloud(){
    global $CLOUD_DAEMON_FILE, $CLOUD_DAEMON_FILE_NAME;
    exec("sudo $CLOUD_DAEMON_FILE start >/dev/null 2>/dev/null &");
    exec("sudo remountrw; sudo update-rc.d $CLOUD_DAEMON_FILE_NAME defaults 90 >/dev/null 2>/dev/null &");
    echo "start ok";
}

function stop_cloud(){
    global $CLOUD_DAEMON_FILE, $CLOUD_DAEMON_FILE_NAME;
    exec("sudo $CLOUD_DAEMON_FILE stop");
    exec("sudo remountrw; sudo update-rc.d -f $CLOUD_DAEMON_FILE_NAME remove >/dev/null 2>/dev/null &");
    echo "stop ok";
}

function save_cloud($values){
    global $CLOUD_CONFIG_FILE, $CLOUD_SECTION, $CLOUD_TOPIC_FILE, $CLOUD_MSG_FILE;
    echo "<script>alert('hello');</script>";
    $config = new Config_Lite($CLOUD_CONFIG_FILE);
    $config->set($CLOUD_SECTION, 'mqttserver',     str_replace("\\'","",$values['host']));
    $config->set($CLOUD_SECTION, 'mqttport',     str_replace("\\'","",$values['port']));
    $config->set($CLOUD_SECTION, 'mqttuser',     str_replace("\\'","",$values['user']));
    $config->set($CLOUD_SECTION, 'mqttpassword',     str_replace("\\'","",$values['pass']));
    $config->set($CLOUD_SECTION, 'client_id',     str_replace("\\'","",$values['client_id']));
    @$config->save();

    $template_file_topic = $_POST['template_file_topic'];
    $template_file_topic = str_replace(array("\\'","\\\"","\\\\"),array("","\"","\\"),$template_file_topic);
    write_template($template_file_topic, $CLOUD_TOPIC_FILE);

    $template_file_message = $_POST['template_file_message'];
    $template_file_message = str_replace(array("\\'","\\\"","\\\\"),array("","\"","\\"),$template_file_message);
    write_template($template_file_message, $CLOUD_MSG_FILE);
    echo "ok";
}

/*
 * Get configuration values from file and stores them in an array.
 */
function get_cloud_config(){
    global $CLOUD_CONFIG_FILE, $CLOUD_SECTION, $CLOUD_TOPIC_FILE, $CLOUD_MSG_FILE;
    global $section, $plugin, $cloud;

    $MQTT_config = new Config_Lite($CLOUD_CONFIG_FILE);
    $config = array(
        'host'       => $MQTT_config->get($CLOUD_SECTION, 'mqttserver','localhost'),
        'port'       => $MQTT_config->get($CLOUD_SECTION, 'mqttport','1883'),
        'user'       => $MQTT_config->get($CLOUD_SECTION, 'mqttuser',''),
        'pass'       => $MQTT_config->get($CLOUD_SECTION, 'mqttpassword',''),
        'qos'        => $MQTT_config->get($CLOUD_SECTION, 'qos' ,'0'),
        'limit'      => $MQTT_config->get($CLOUD_SECTION, 'limit' ,'200'),
        'interval'   => $MQTT_config->get($CLOUD_SECTION, 'interval' ,'60'),
        'client_id'  => $MQTT_config->get($CLOUD_SECTION, 'client_id' ,'')
        );

    $template_topic_file = getTemplateFile($CLOUD_TOPIC_FILE);
    $template_message_file = getTemplateFile($CLOUD_MSG_FILE);

    $tooltip_topic_text = "Set a topic template structure to parse your data, replace wildcards:\n\n" .
        "- #MESHLIUM# : Identifier for Meshlium\n" .
        "- #ID# : Unique identifier for data\n" .
        "- #ID_WASP# : Identifier for Wapmote\n" .
        "- #SENSOR# : Sensor identification\n";

    $tooltip_message_text = "Set a message template structure to parse your data, replace wildcards:\n\n" .
        "- #ID# : Unique identifier for data\n" .
        "- #ID_WASP# : Identifier for Wapmote\n" .
        "- #ID_SECRET# : Secret idenitifier\n" .
        "- #SENSOR# : Sensor identification\n" .
        "- #VALUE# : Value for sensor\n" .
        "- #TIMESTAMP# : Date in custom format - yyyy-mm-dd h:m:s\n";



    $html='
    <tr><td>IP Address: </td><td><input autocomplete="off" id="address" type="text" name="host" value="'. $config['host'] .'" /> </td></tr>
    <tr><td>Port number: </td><td><input autocomplete="off" id="port_number" type="text" name="port" value="'. $config['port'] .'" /> </td></tr>
    <tr><td>User: </td><td><input autocomplete="off" id="user" type="text" name="user" value="'. $config['user'] .'" /> </td></tr>
    <tr><td>Password: </td><td><input autocomplete="off" id="password" type="password" name="pass" value="'. $config['pass'] .'" /> </td></tr>
    <tr><td>Client ID: </td><td><input autocomplete="off" id="client_id" type="text" name="client_id" value="'. $config['client_id'] .'" /> </td></tr>
    <tr><td><label>Topic template:</label></td>
    <td>
        <textarea class="ms_hex" name="template_file_topic" id="template_file_topic" title="' . $tooltip_topic_text . '">'. $template_topic_file .'</textarea>
    </td></tr>
    <tr><td><label>Message template: </label></td>
    <td>
        <textarea class="ms_hex" name="template_file_message" id="template_file_message" title=\'' . $tooltip_message_text . '\'>'. $template_message_file .'</textarea>
    </td></tr>
    <tr><td colspan="2" align="center">
        <button type="button" class="save_button" onclick="save_cloud(\''.$section.'\',\''.$plugin.'\',\''.$cloud.'\')"><i class="fa fa-floppy-o"></i>  Save</button>
    </td></tr>';

    return $html;
}


function getTemplateFile($path){
    if ($path == "")
        return "";

    $template_string = file_get_contents($path, FILE_USE_INCLUDE_PATH);
    $template_string_clean = str_replace(array("\\\"","\\\\"),array("\"","\\"),$template_string);

    return $template_string_clean;
}

function write_template($template_file, $path = "") {
    if ($path == "") return false;
    $writepath = $path;
    exec("sudo remountrw");
    $fp = fopen($writepath, "w");
    fwrite($fp, $template_file);
    fclose($fp);
    exec("sudo remountro");
}

?>
