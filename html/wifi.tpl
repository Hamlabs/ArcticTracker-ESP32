 
<html>
%head%
<body>
<h2>WIFI configuration</h2>
<form method="post" action="/wifiupdate">
<fieldset>
  
  <label class="leftlab">Soft AP SSID:</label><label id="apssid">%apssid%</label>
  <label class="leftlab">Soft AP passwd:</label>
  <input type="password" id="appass" name="appass" size="15" value="%appass%" />
  <br><br>
  
  <label class="leftlab">Webserver user:</label>
  <input type="text" id="htuser" name="htuser" size="15" value="%htuser%" />
  <br>
  <label class="leftlab">Webserver passwd:</label>
  <input type="password" id="htpass" name="htpass" size="15" value="%htpass%" />
  <br><br>
  
  <label class="leftlab">AP 1 (ssid,passwd):</label>
  <input type="text" id="wifiap0_ssid" name="wifiap0_ssid" size="15" value="%ssid0%" />
  <input type="password" id="wifiap0_pwd" name="wifiap0_pwd" size="15" value="%passwd0%" /><br>

  <label class="leftlab">AP 2 (ssid,passwd):</label>
  <input type="text" id="wifiap1_ssid" name="wifiap1_ssid" size="15" value="%ssid1%" />
  <input type="password" id="wifiap1_pwd" name="wifiap1_pwd" size="15" value="%passwd1%" /><br>

  <label class="leftlab">AP 3 (ssid,passwd):</label>
  <input type="text" id="wifiap2_ssid" name="wifiap2_ssid" size="15" value="%ssid2%" />
  <input type="password" id="wifiap2_pwd" name="wifiap2_pwd" size="15" value="%passwd2%" /><br>

  <label class="leftlab">AP 4 (ssid,passwd):</label>
  <input type="text" id="wifiap3_ssid" name="wifiap3_ssid" size="15" value="%ssid3%" />
  <input type="password" id="wifiap3_pwd" name="wifiap3_pwd" size="15" value="%passwd3%" /><br>

  <label class="leftlab">AP 5 (ssid,passwd):</label>
  <input type="text" id="wifiap4_ssid" name="wifiap4_ssid" size="15" value="%ssid4%" />
  <input type="password" id="wifiap4_pwd" name="wifiap4_pwd" size="15" value="%passwd4%" /><br>

  <label class="leftlab">AP 6 (ssid,passwd):</label>
  <input type="text" id="wifiap5_ssid" name="wifiap5_ssid" size="15" value="%ssid5%" />
  <input type="password" id="wifiap5_pwd" name="wifiap5_pwd" size="15" value="%passwd5%" /><br>
  <br>
  
</fieldset>
<button type="submit" name="update" id="update">Update</button>
</form>
</body>
</html>
