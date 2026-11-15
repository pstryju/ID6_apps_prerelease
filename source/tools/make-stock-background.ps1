param([Parameter(Mandatory=$true)][string]$OutputPath)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
# Reconstruct the RHMI cards from geometry and text; no packaged bitmap input.
$image=[Drawing.Bitmap]::new(1172,400,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics=[Drawing.Graphics]::FromImage($image)
$fill=[Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#181d23'))
$ink=[Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#b5bfcb'))
$font=[Drawing.Font]::new('Arial Narrow',25,[Drawing.FontStyle]::Regular,[Drawing.GraphicsUnit]::Pixel)
$small=[Drawing.Font]::new('Arial Narrow',24,[Drawing.FontStyle]::Regular,[Drawing.GraphicsUnit]::Pixel)
try {
    $graphics.Clear([Drawing.Color]::Transparent)
    $graphics.SmoothingMode=[Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.TextRenderingHint=[Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $titles=@('BOOST','AFR','TIMING','INTAKE AIR','RPM','RAIL PRESSURE','OIL PRESSURE','ENGINE TEMP')
    for($i=0;$i -lt 9;$i++) {
        if($i -eq 8){$x=1032;$y=0;$w=140;$h=400;$title='OIL TEMP';$type=$small}
        else{$x=258*[int][Math]::Floor($i/2);$y=208*($i%2);$w=240;$h=192;$title=$titles[$i];$type=$font}
        $path=[Drawing.Drawing2D.GraphicsPath]::new()
        try {
            $path.AddArc($x,$y,32,32,180,90)
            $path.AddArc($x+$w-32,$y,32,32,270,90)
            $path.AddArc($x+$w-32,$y+$h-32,32,32,0,90)
            $path.AddArc($x,$y+$h-32,32,32,90,90)
            $path.CloseFigure()
            $graphics.FillPath($fill,$path)
            $graphics.DrawString($title,$type,$ink,[single]($x+14),[single]($y+17))
        } finally{$path.Dispose()}
    }
    $null=[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($OutputPath)))
    $image.Save($OutputPath,[Drawing.Imaging.ImageFormat]::Png)
} finally{$small.Dispose();$font.Dispose();$ink.Dispose();$fill.Dispose();$graphics.Dispose();$image.Dispose()}
