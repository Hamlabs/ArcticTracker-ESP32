 
 
<html>
%head%
<body>
<h2>Firmware update</h2>


<form method="post" action="/fwupdate">
<fieldset class="fw">

<label>Download URL:</label>
<input class="fwup" type="text" id="fw_url" name="fw_url" pattern=".*" value="%fw_url%" /> 
<br><p class="expl">
For security, server certificate will be verified. You may add the issuer (CA) certificate here (PEM format), or, if this field is left empty, the most common issuers will be checked.
</p>
<label>CA certificate:</label>
<textarea class="fwup" id="fw_cert" name="fw_cert" pattern=".*">
%fw_cert%
</textarea>
<br>

</fieldset>
<button type="submit" name="update" id="update">Update</button>
</form>
</body>
</html>
