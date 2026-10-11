# grab.ps1
# Запускать: правой кнопкой -> "Выполнить с помощью PowerShell"
# либо:      powershell -ExecutionPolicy Bypass -File .\grab.ps1

# Каталог, где лежит сам скрипт
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $Root

# --- Определяем имя проекта ---
$ProFile = Get-ChildItem -LiteralPath $Root -Filter *.pro -File | Select-Object -First 1
if ($ProFile) {
    $ProjectName = [IO.Path]::GetFileNameWithoutExtension($ProFile.Name)
} else {
    $ProjectName = Split-Path -Leaf $Root
}

$OutFile = Join-Path $Root "$ProjectName.grab.txt"
if (Test-Path -LiteralPath $OutFile) { Remove-Item -LiteralPath $OutFile -Force }

# --- Расширения исходников Qt ---
$Exts = @('.pro','.pri','.cpp','.c','.cc','.cxx','.h','.hpp','.hxx',
          '.ui','.qrc','.qml','.js','.ts')

Write-Host "Сканирую каталог: $Root"
Write-Host "Имя проекта:      $ProjectName"
Write-Host "Выходной файл:    $OutFile"
Write-Host ""

# --- Обход рекурсивно всех файлов ---
$Files = Get-ChildItem -LiteralPath $Root -Recurse -File |
         Where-Object {
             $Exts -contains $_.Extension.ToLower() -and
             $_.FullName -ne $OutFile
         } |
         Sort-Object FullName

$Writer = New-Object System.IO.StreamWriter($OutFile, $false, [System.Text.UTF8Encoding]::new($false))

try {
    foreach ($f in $Files) {
        $rel = $f.FullName.Substring($Root.Length).TrimStart('\','/')
        Write-Host "  + $rel"

        $Writer.WriteLine("Файл $rel")
        $Writer.WriteLine("--------------------------")

        # Пишем содержимое файла как есть
        $content = Get-Content -LiteralPath $f.FullName -Raw -ErrorAction SilentlyContinue
        if ($null -ne $content) {
            # Сохраняем исходные переводы строк
            $Writer.Write($content)
            if (-not $content.EndsWith("`n")) { $Writer.WriteLine() }
        }
        $Writer.WriteLine()
    }
}
finally {
    $Writer.Close()
}

Write-Host ""
Write-Host "Готово: $OutFile"