param(
    [string]$SourceRoot = 'D:\世嘉MD游戏3400款全集\世嘉MD游戏3400款全集\roms',
    [string]$OutputRoot = (Join-Path $PSScriptRoot '..\tmp\md-test-pack')
)

$ErrorActionPreference = 'Stop'

$games = @(
    @{ Id='01'; File='索尼克1.zip';                         Out='sonic_1.bin';               Title='索尼克1';             Category='动作'; Tier='core';      Controls='3-button'; Purpose='基础速度与卷轴' },
    @{ Id='02'; File='索尼克2.zip';                         Out='sonic_2.bin';               Title='索尼克2';             Category='动作'; Tier='core';      Controls='3-button'; Purpose='高速卷轴与双角色' },
    @{ Id='03'; File='索尼克3.zip';                         Out='sonic_3.bin';               Title='索尼克3';             Category='动作'; Tier='core';      Controls='3-button'; Purpose='2MB边界与存档' },
    @{ Id='04'; File='怒之铁拳1.zip';                        Out='streets_of_rage_1.bin';     Title='怒之铁拳1';           Category='清版'; Tier='core';      Controls='3-button'; Purpose='多角色与音效' },
    @{ Id='05'; File='怒之铁拳2[美].zip';                    Out='streets_of_rage_2.bin';     Title='怒之铁拳2';           Category='清版'; Tier='core';      Controls='3-button'; Purpose='YM2612音乐压力' },
    @{ Id='06'; File='怒之铁拳3[美].zip';                    Out='streets_of_rage_3.bin';     Title='怒之铁拳3';           Category='清版'; Tier='memory-3m'; Controls='3-button'; Purpose='3MB内存压力' },
    @{ Id='07'; File='战斧1.zip';                            Out='golden_axe_1.bin';          Title='战斧1';               Category='清版'; Tier='core';      Controls='3-button'; Purpose='基础兼容性' },
    @{ Id='08'; File='光明与黑暗众神的遗产中文版.zip';          Out='shining_force_1_zh.smd';   Title='光明力量1（中）';       Category='角色扮演'; Tier='core-smd';  Controls='3-button'; Purpose='经典交织SMD与SRAM' },
    @{ Id='09'; File='战斧3.zip';                            Out='golden_axe_3.bin';          Title='战斧3';               Category='清版'; Tier='core';      Controls='3-button'; Purpose='角色与区域兼容' },
    @{ Id='10'; File='火枪英雄[美].zip';                     Out='gunstar_heroes.bin';        Title='火枪英雄';             Category='射击'; Tier='core';      Controls='3-button'; Purpose='大量精灵与旋转效果' },
    @{ Id='11'; File='魂斗罗-铁血军团[美].zip';               Out='contra_hard_corps.bin';     Title='魂斗罗：铁血军团';     Category='射击'; Tier='core';      Controls='3-button'; Purpose='高负载动作场景' },
    @{ Id='12'; File='恶魔城-血族.zip';                       Out='castlevania_bloodlines.bin';Title='恶魔城：血族';         Category='动作'; Tier='core';      Controls='3-button'; Purpose='特殊卷轴与画面效果' },
    @{ Id='13'; File='雷电.zip';                             Out='raiden.bin';                Title='雷电';                 Category='射击'; Tier='core';      Controls='3-button'; Purpose='纵向滚动与弹幕' },
    @{ Id='14'; File='闪电出击4.zip';                        Out='thunder_force_4.bin';       Title='闪电出击4';            Category='射击'; Tier='core';      Controls='3-button'; Purpose='高强度横向卷轴与音乐' },
    @{ Id='15'; File='暴力摩托2美欧.zip';                     Out='road_rash_2.bin';           Title='暴力摩托2';            Category='竞速'; Tier='core';      Controls='3-button'; Purpose='伪SMD后缀与道路缩放' },
    @{ Id='16'; File='漫画地带[美].zip';                     Out='comix_zone.bin';            Title='漫画地带';             Category='动作'; Tier='core';      Controls='3-button'; Purpose='复杂画面与音频' },
    @{ Id='17'; File='光明力量2-古代之封印[美].zip';           Out='shining_force_2.bin';       Title='光明力量2';            Category='角色扮演'; Tier='core';      Controls='3-button'; Purpose='SRAM与长时间运行' },
    @{ Id='18'; File='梦幻模拟战2 (简) (狼组).zip';            Out='langrisser_2.md';           Title='梦幻模拟战2（简）';     Category='策略'; Tier='core';      Controls='3-button'; Purpose='中文ROM与MD扩展名' },
    @{ Id='19'; File='梦幻之星4-千年纪的终结[美].zip';          Out='phantasy_star_4.bin';       Title='梦幻之星4';            Category='角色扮演'; Tier='memory-3m'; Controls='3-button'; Purpose='3MB、SRAM与长时间运行' },
    @{ Id='20'; File='超级街头霸王2美.zip';                    Out='super_street_fighter_2.bin';Title='超级街头霸王2';        Category='格斗'; Tier='memory-5m'; Controls='6-button'; Purpose='5MB极限、伪SMD后缀与六键输入' }
)

if (-not (Test-Path -LiteralPath $SourceRoot -PathType Container)) {
    throw "ROM source directory not found: $SourceRoot"
}
if (Test-Path -LiteralPath $OutputRoot) {
    throw "Output already exists; choose a new path or remove it explicitly: $OutputRoot"
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$romExtensions = @('.bin', '.smd', '.md', '.gen', '.mdx')
$romDir = Join-Path $OutputRoot 'roms'
New-Item -ItemType Directory -Path $romDir | Out-Null

$manifest = @()
foreach ($game in $games) {
    $archivePath = Join-Path $SourceRoot $game.File
    if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
        throw "Selected archive not found: $archivePath"
    }

    $archive = [System.IO.Compression.ZipFile]::OpenRead($archivePath)
    try {
        $romEntries = @($archive.Entries | Where-Object {
            -not [string]::IsNullOrEmpty($_.Name) -and
            $romExtensions -contains [System.IO.Path]::GetExtension($_.Name).ToLowerInvariant()
        })
        if ($romEntries.Count -ne 1) {
            throw "Expected exactly one MD ROM in $($game.File), found $($romEntries.Count)"
        }

        $entry = $romEntries[0]
        $outputPath = Join-Path $romDir $game.Out
        $source = $entry.Open()
        $destination = [System.IO.File]::Create($outputPath)
        try {
            $source.CopyTo($destination)
        } finally {
            $destination.Dispose()
            $source.Dispose()
        }

        $header = [System.IO.File]::ReadAllBytes($outputPath)
        $outputExtension = [System.IO.Path]::GetExtension($outputPath).ToLowerInvariant()
        if ($outputExtension -eq '.smd') {
            $validHeader = $header.Length -gt 512 -and
                $header[1] -eq 0x03 -and $header[8] -eq 0xAA -and $header[9] -eq 0xBB
        } else {
            $validHeader = $header.Length -ge 0x104 -and
                [System.Text.Encoding]::ASCII.GetString($header, 0x100, 4) -eq 'SEGA'
        }
        if (-not $validHeader) {
            throw "ROM content does not match normalized output format: $($game.File) -> $($game.Out)"
        }

        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $outputPath).Hash
        $manifest += [PSCustomObject]@{
            id = $game.Id
            filename = $game.Out
            title = $game.Title
            category = $game.Category
            tier = $game.Tier
            controls = $game.Controls
            purpose = $game.Purpose
            source_zip = $game.File
            source_entry = $entry.Name
            bytes = $entry.Length
            sha256 = $hash
        }
    } finally {
        $archive.Dispose()
    }
}

$manifestPath = Join-Path $OutputRoot 'catalog.tsv'
$manifest | Export-Csv -LiteralPath $manifestPath -Delimiter "`t" -NoTypeInformation -Encoding utf8

$readme = @"
QDTech MD/Gwenesis local compatibility test pack

- Generated from the user's local ROM collection.
- Contains 20 representative ROMs only; never commit or upload this directory.
- ROM files are extracted so each test item contains no bundled readme or URL.
- core/core-smd: first compatibility gate (<= 2 MiB).
- memory-3m: run only after the core tier is stable.
- memory-5m: final stress test; failure does not block the core tier.
- 32X titles are intentionally excluded.
"@
[System.IO.File]::WriteAllText((Join-Path $OutputRoot 'README.txt'), $readme, [System.Text.UTF8Encoding]::new($false))

Write-Host "Prepared $($manifest.Count) MD test ROMs at $OutputRoot"
$manifest | Select-Object id, title, tier, controls, bytes, filename | Format-Table -AutoSize
