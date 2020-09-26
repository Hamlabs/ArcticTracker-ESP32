 
 
<html>
%head%
<body>
<h2>Track logging configuration</h2>


<form method="post" action="/trklogupdate">
<fieldset>

<label class="leftlab"><b>Track logging:</b></label>
<input type="checkbox" id="trklog_on" name="trklog_on" value="false" %trklog_on%/>Activate
<br>
<br>

<label class="leftlab">Save interval:</label>
<input type="text" id="tl_int" name="tl_int" size="3" pattern="[0-9]+" value="%tlog_int%" />
(seconds)
<br>
<label class="leftlab">Time to live:</label>
<input type="text" id="tl_ttl" name="tl_ttl" size="3" pattern="[0-9]+" value="%tlog_ttl%" />
(days)
<br>
<br>

<label class="leftlab"><b>Track log server:</b></label>
<input type="text" id="tl_host" name="tl_host" size="30" pattern="[a-zA-Z0-9\-\.]+" value="%tlog_host%" /> 
<br>
<label class="leftlab">Port number:</label>
<input type="text" id="tl_port" name="tl_port" size="6" pattern="[0-9]+" value="%tlog_port%" />
<br>
<label class="leftlab">Path:</label>
<input type="text" id="tl_path" name="tl_path" size="20" pattern="[a-zA-Z0-9\-\.\/]+" value="%tlog_path%" />
<br>

</fieldset>
<button type="submit" name="update" id="update">Update</button>
</form>
</body>
</html>
