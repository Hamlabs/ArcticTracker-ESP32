 
 
<html>
%head%
<body>
<h2>Track logging configuration</h2>


<form method="post" action="/trklogupdate">
<fieldset>

<label class="leftlab"><b>Track logging:</b></label>
<input type="checkbox" id="trklog_on" name="trklog_on" value="true" %trklog_on%/>Activate
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

<label class="leftlab"><b>Auto upload:</b></label>
<input type="checkbox" id="trkpost_on" name="trkpost_on" value="true" %trkpost_on%/>Activate
<br>

<label class="leftlab">Server URL:</label>
<input type="text" id="tl_host" name="tl_url" size="40" pattern="(http|https):\/\/[a-zA-Z0-9\-\_\.\/]+" value="%tlog_url%" /> 
<br>
<label class="leftlab">Server Key:</label>
<input type="text" id="tl_key" name="tl_key" size="40" pattern=".+" value="%tlog_key%" />
<br>

</fieldset>
<button type="submit" name="update" id="update">Update</button>
</form>
</body>
</html>
