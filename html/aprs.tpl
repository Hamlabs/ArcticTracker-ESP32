 
 
<html>
%head%
<body>
<h2>APRS configuration</h2>


<form method="post" action="/aprsupdate">
<fieldset>

<label class="leftlab">My callsign:</label>
<input type="text" id="mycall" name="mycall" size="10" pattern="[a-zA-Z0-9\-]+" value="%mycall%" /> 
<br>
<label class="leftlab">Symbol (tab/sym):</label>
<input type="text" id="symbol" name="symbol" size="2" pattern=".." value="%symbol%" />
<br>
<label class="leftlab">Report comment:</label>
<input type="text" id="rcomment" name="rcomment" size="30" pattern=".*" value="%comment%" />
<br>
<label class="leftlab">Digipeater path:</label>
<input type="text" id="digis" name="digis" size="30" pattern="([a-zA-Z0-9\-]+)(\,([a-zA-Z0-9\-]+)*" value="%digipath%" />
<br>
<label class="leftlab">TX frequency:</label>
<input type="text" id="tx_freq" name="tx_freq" size="10" pattern="[0-9]+" value="%txfreq%" />
<br>
<label class="leftlab">RX frequency:</label>
<input type="text" id="rx_freq" name="rx_freq" size="10" pattern="[0-9]+" value="%rxfreq%" />
<br><br>
 
<label class="leftlab">Tracking attributes:</label>
<input type="checkbox" id="timestamp_on" name="timestamp_on" value="true" %timestamp_on%/>Timestamp
<input type="checkbox" id="compress_on" name="compress_on" value="true" %compress_on%/>Compress
<input type="checkbox" id="altitude_on" name="altitude_on" value="true" %altitude_on%/>Altitude
<br>

<label class="leftlab">Turn limit:</label>
<input type="text" id="turnlimit" name="turnlimit" size="5" pattern="[0-9]+" value="%turnlimit%" /> (degrees)
<br>
<label class="leftlab">Max pause:</label>
<input type="text" id="maxpause" name="maxpause" size="5" pattern="[0-9]+" value="%maxpause%" /> (seconds)
<br>
<label class="leftlab">Min pause:</label>
<input type="text" id="minpause" name="minpause" size="5" pattern="[0-9]+" value="%minpause%" /> (seconds)
<br>
<label class="leftlab">Min distance:</label>
<input type="text" id="mindist" name="mindist" size="5" pattern="[0-9]+" value="%mindist%" /> (meters)
<br><br>
<label class="leftlab"><b>Extra posreports:</b></label>
<input type="checkbox" id="xonturn_on" name="xonturn_on" value="true" %xonturn_on%/> Add when turning
<br>
<label class="leftlab">Redundant reports (0-4):</label>
<input type="text" id="redundancy" name="redundancy" size="5" pattern="[0-9]+" value="%redundancy%" /> 

</fieldset>
<button type="submit" name="update" id="update">Update</button>
</form>
</body>
</html>
