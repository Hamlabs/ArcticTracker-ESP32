 
 
<html>
%head%
<body>
<h2>Digipeater/Igate configuration</h2>


<form method="post" action="/digiupdate">
<fieldset>

<label class="leftlab"><b>Digipeater:</b></label>
<input type="checkbox" id="digi_on" name="digi_on" value="true" %digi_on%/>Activate
<br>
<label class="leftlab">Digipeating modes:</label>
<input type="checkbox" id="wide1_on" name="wide1_on" value="true" %wide1_on%/>Wide-1 (fill-in)
<input type="checkbox" id="sar_on" name="sar_on" value="true" %sar_on%/>Preemption on 'SAR'
<br><br>
<label class="leftlab"><b>Internet gate:</b></label>
<input type="checkbox" id="igate_on" name="igate_on" value="true" %igate_on%/>Activate
<br>

<label class="leftlab">APRS/IS server:</label>
<input type="text" id="ig_host" name="ig_host" size="30" pattern="[a-zA-Z0-9\-\.]+" value="%igate_host%" /> 
<br>
<label class="leftlab">Port number:</label>
<input type="text" id="ig_port" name="ig_port" size="6" pattern="[0-9]+" value="%igate_port%" />
<br>
<label class="leftlab">Username:</label>
<input type="text" id="ig_user" name="ig_user" size="10" pattern="[a-zA-Z0-9\-\.]+" value="%igate_user%" /> 
<br>
<label class="leftlab">Passcode:</label>
<input type="text" id="ig_pass" name="ig_pass" size="6" pattern="[0-9]+" value="%igate_pass%" />
<br>

</fieldset>
<button type="submit" name="update" id="update">Update</button>
</form>
</body>
</html>
