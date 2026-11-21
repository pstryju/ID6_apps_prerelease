# Komendy projektu

Ściąga dla naszego portu **NBTevo_Y24073J, 1280×480**. Polecenia uruchamiaj
w **PowerShellu**, z głównego katalogu repozytorium:

```powershell
cd C:\Users\pawel\Developer\ID6_apps
```

## Najczęściej używane

| Cel | Komenda |
| --- | --- |
| Sprawdzenie SSH | `.\Test-Evo-SSH.cmd` |
| Instalacja BoostDeck + ShiftDeck | `.\Install-Evo.cmd` |
| Instalacja Engine Fault Codes | `.\Install-Evo.cmd -Apps EngineCodes` |
| HUD z łukiem z main, RPM z CDS | `.\Install-Evo.cmd -Apps HudTest -HudVariant UpstreamCds` |
| Oryginalny HUD z main, odczyty HSFZ | `.\Install-Evo.cmd -Apps HudTest -HudVariant Upstream` |
| Instalacja xDrive Control | `.\Install-Evo.cmd -Apps XDriveControl` |
| Instalacja LaunchDeck, Road Jet i BEAMer | `.\Install-Evo.cmd -AppSet Activities` |
| Instalacja LaunchDeck | `.\Install-Evo.cmd -Apps LaunchDeck` |
| Instalacja Road Jet | `.\Install-Evo.cmd -Apps RoadJet` |
| Instalacja BEAMer | `.\Install-Evo.cmd -Apps BEAMer` |
| Przywrócenie wersji sprzed ostatniej instalacji | `.\Uninstall-Evo.cmd` |
| Podgląd instalacji bez łączenia z autem | `.\Install-Evo.cmd -WhatIf` |
| Usunięcie starych AbiSmoke/CdsProbe | `.\Remove-Evo-Tests.cmd` |

Instalacja i deinstalacja nie restartują HMI. Po pomyślnym zakończeniu wykonaj
ręczny restart iDrive, przytrzymując pokrętło głośności, aż ekran zgaśnie.
Na czas działań w aucie auto ma stać, a Evo musi być obudzone. Skrypty pytają
o potwierdzenie; parametr `-Parked` oznacza, że potwierdzasz te warunki z góry.

Domyślne IP Evo to **169.254.199.119**, a gateway diagnostyczny to
**169.254.20.21**. Hasło jest pobierane z lokalnego, ignorowanego przez Git
pliku `.local/evo-auth.ps1`; jeśli go nie ma, skrypt zapyta o hasło.
Instalator sprawdza kompletność plików i sumy SHA-256 — najpierw wykonaj build.

## Zestawy aplikacji i profile silnika

```powershell
.\Install-Evo.cmd -AppSet Gauges
.\Install-Evo.cmd -AppSet Core
.\Install-Evo.cmd -AppSet All
.\Install-Evo.cmd -AppSet Extras
```

| Zestaw | Aplikacje |
| --- | --- |
| `Gauges` — domyślny | BoostDeck, ShiftDeck |
| `Core` | ShiftDeck, EngineCodes |
| `All` | BoostDeck, ShiftDeck, EngineCodes |
| `Extras` | HudTest, XDriveControl, LaunchDeck, RoadJet, BEAMer |
| `Activities` | LaunchDeck, RoadJet, BEAMer |

`All` nie obejmuje `Extras`. Parametr `-Apps` ma pierwszeństwo przed zestawem.
Instalacja xDrive dodaje menu, ale nie przełącza napędu. HUD jest domyślnie
wyłączony i wymaga włączenia w aplikacji.

```powershell
# Obecny N20 — także domyślna konfiguracja bez podania parametru:
.\Install-Evo.cmd -EngineProfile n20

# Eksperymentalny wybór profilu na podstawie identyfikacji DME:
.\Install-Evo.cmd -EngineProfile auto
```

`auto` nie jest jeszcze potwierdzone w aucie. Przy braku jednoznacznej
identyfikacji pozostawia odczyty diagnostyczne puste; CDS działa niezależnie.
Profile `b48` i `n13` są przygotowane w rejestrze, ale nie mają jeszcze
zweryfikowanego mapowania. Dla obecnego auta pozostaw `n20`.
Szczegóły: [profile silników](source/docs/ENGINE-PROFILES.md).

Zmiana adresów, jeśli kiedyś będzie potrzebna:

```powershell
.\Install-Evo.cmd -Head 169.254.199.119 -Gateway 169.254.20.21
```

Gateway dotyczy EngineCodes i xDrive. BoostDeck korzysta bezpośrednio z MOST,
a ShiftDeck z CDS.

## Deinstalacja i cofanie wdrożeń

```powershell
# Cofnij ostatnią zarejestrowaną instalację dla tego Evo:
.\Uninstall-Evo.cmd

# Najpierw wyświetl, które wdrożenie zostanie cofnięte:
.\Uninstall-Evo.cmd -WhatIf

# Cofnij wskazane wdrożenie — zastąp ID_WDROZENIA prawdziwym Deployment ID:
.\Uninstall-Evo.cmd -RunId ID_WDROZENIA

# Usuń wycofane aplikacje testowe na podstawie ich osobnych potwierdzeń:
.\Remove-Evo-Tests.cmd
```

Deinstalacja cofa **konkretne wdrożenie**, a nie wszystkie aplikacje naraz.
Przywraca wcześniejsze foldery z kopii; jeśli przed instalacją aplikacji nie
było, usuwa ją z katalogu Plugins. Kolejne wdrożenia cofaj od najnowszego.

Potwierdzenia są w `.local/deployments/`. Deployment ID i ścieżkę kopii na Evo
wypisuje instalator. Nie kasuj tych danych przed zakończeniem testów.

## Build i kontrola zgodności — offline

Uruchomiony Docker Desktop, lokalny obraz `qnx650-sdk`, licencjonowany SDK QNX
i skopiowane biblioteki Evo są wymagane do builda. Domyślne katalogi na tym
komputerze to `C:\Users\pawel\evo` oraz
`C:\Users\pawel\Developer\qnx650-docker`.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\source\tools\build-dashboard.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\source\tools\check-dashboard.ps1
.\Install-Evo.cmd -AppSet All -WhatIf
.\Install-Evo.cmd -AppSet Extras -WhatIf
```

Build przygotowuje pliki w `source/native/out/dashboard/candidate/` i sumy
w `source/native/out/dashboard/SHA256SUMS`. Nie łączy się z autem i niczego
nie instaluje. Kontrola zgodności porównuje importy z lokalnymi bibliotekami
Evo; nie zastępuje testu działania w aucie.

Inne lokalizacje można podać jawnie:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\source\tools\build-dashboard.ps1 -EvoDirectory C:\Users\pawel\evo -QnxDirectory C:\Users\pawel\Developer\qnx650-docker -Image qnx650-sdk
```

## Testy automatyczne — offline

Test transportu Windows i przygotowanie lokalnych fixture'ów wdrażania:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\source\tools\test_deploy.ps1
```

Testy hostowe w przygotowanym lokalnie obrazie `nbt-host-tests-full`:

```powershell
$docker = "$env:LOCALAPPDATA\Programs\DockerDesktop\resources\bin\docker.exe"
& $docker run --rm --network none --mount "type=bind,source=$PWD,target=/work" -w /work/source nbt-host-tests-full sh -c 'make -C native test && python3 tools/test_probe_signals.py && python3 tools/test_capture_headunit.py && python3 tools/test_capture_runtime.py && python3 native/tests/test_deploy.py && python3 native/tests/test_bundle.py && python3 native/tests/test_batch_deploy.py && python3 native/tests/test_gateway_config.py'
```

Testy wdrażania wymagają fixture'ów wygenerowanych pierwszym poleceniem.
Żaden z tych testów nie używa auta. Jeżeli pracujesz bezpośrednio na Linuksie
z kompilatorem C/C++, Make i Pythonem, podstawowe testy uruchomisz z katalogu
`source/` poleceniem `make -C native test`.

## Diagnostyka w aucie — tylko odczyty

Przed probe'ami zamknij aplikacje zegarów i inne testery diagnostyczne.
Polecenia przesyłają tymczasowy program na Evo, ale nie instalują aplikacji
w HMI, nie kasują błędów i nie restartują jednostki.

| Cel | Komenda |
| --- | --- |
| Znane parametry DME przez MOST | `.\Test-Evo-MOST.cmd` |
| Ślad inicjalizacji HUD przez navi | **Zablokowany po awarii procesu i restarcie Evo — nie uruchamiać** |
| Identyfikacja DME do doboru profilu | `.\Test-Evo-Engine.cmd` |
| Identyfikacja i odczyt kandydata klapy wydechu | `.\Test-Evo-Exhaust.cmd` |
| Podstawowe odczyty przez gateway HSFZ | `.\Test-Evo-Diagnostics.cmd` |
| Rozszerzone odczyty HSFZ | `.\Test-Evo-Diagnostics.cmd -Signals` |

`Test-Evo-Exhaust.cmd` **nie otwiera ani nie zamyka klapy**. Sterowanie klapą
N20 pozostaje niepotwierdzone. `Test-Evo-Engine.cmd` odczytuje wyłącznie
`22 F150/F187/F189`, bez pytania o VIN.

Wyniki probe'ów trafiają do `.local/diagnostics/`. Domyślnie test HSFZ sprawdza
kilka adresów; ograniczenie do naszego gatewaya:

```powershell
.\Test-Evo-Diagnostics.cmd -Gateways 169.254.20.21
```

MOST działa wewnątrz Evo, ale do uruchomienia probe'a z laptopa potrzebujesz
połączenia SSH z jednostką.

## Ręczne SSH

```powershell
ssh -o HostKeyAlgorithms=+ssh-rsa -o MACs=+hmac-sha1 root@169.254.199.119
ssh -o HostKeyAlgorithms=+ssh-rsa -o MACs=+hmac-sha1 root@169.254.199.99
```

Pierwszy adres to OMAP z HMI, drugi to Jacinto. Instalację aplikacji kieruj
na OMAP; sam dostęp SSH do Jacinto nie oznacza dostępu do właściwego katalogu
Plugins. Hasła nie wpisuj w argumenty komendy.

Więcej informacji: [testy w aucie](TESTY-W-AUCIE.md),
[BoostDeck MOST](source/docs/BOOSTDECK-MOST.md),
[HUD, xDrive i wydech](source/docs/HUD-XDRIVE-EXHAUST.md).

LaunchDeck, Road Jet i BEAMer są budowane z lokalnymi bibliotekami Evo.
`Activities` dodaje tylko ich menu; BEAMer zaczyna sekwencję dopiero po
wybraniu jej w aplikacji i sprawdzeniu obsługiwanego modułu, P oraz postoju.
Nowe aplikacje korzystają z diagnostyki HSFZ/MOST według kodu upstream,
a nie z naszego źródła CDS. MOST adresuje tylko DME: nie zastępuje odczytu
kąta kierownicy z ICM ani sterowania światłami przez FEM. Nie są jeszcze
sprawdzone na tym aucie.

Po instalacji zestawu `Activities`, `.\Uninstall-Evo.cmd` przywraca stan
sprzed tej instalacji wszystkich trzech aplikacji. Wyniki LaunchDeck i jego
preferencje są przechowywane poza folderem Plugins i pozostają po deinstalacji.
## Konfigurowanie kafelków BoostDeck

Po przebudowaniu pakietu zainstaluj BoostDeck razem z nowym workerem:

```powershell
.\Install-Evo.cmd -Apps BoostDeck
```

W aplikacji wybierz **OPTIONS -> Configure gauges -> Gauge 1..9** i przypisz
odczyt do każdej pozycji. Układy benzyny i Diesla oraz kolor są zapisywane poza
folderem aplikacji. Domyślnie pozostają nasze odczyty, z RPM i temperaturą oleju.
Jednostki temperatury i prędkości są pobierane z Evo; ręcznych opcji jednostek
nie ma. Wybranie pozycji w menu nie gwarantuje, że DME udostępnia ten odczyt.
## HUD z głównego repozytorium

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File source/tools/build-hud-upstream.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File source/tools/check-hud-upstream.ps1
.\Install-Evo.cmd -Apps HudTest -HudVariant UpstreamCds
```

`UpstreamCds` zachowuje kod rysowania łuku 0–8000 RPM i menu z main, a RPM
pobiera przez CDS. Odczyt boost nie jest dostępny w tym wariancie: oryginalne
pole pokaże `--`. Nie uruchamia połączenia HSFZ ani dodatkowej diagnostyki DME.
Checkbox zapisuje stan zgodnie z implementacją main i przywraca go po restarcie;
ponowna instalacja całego folderu może zresetować zapisany stan.

`-HudVariant Upstream` instaluje pliki z `packages/hud-gauges.zip` bez zmian.
Ten oryginał odczytuje RPM i boost przez HSFZ, więc wymaga działającego gatewaya.
`-HudVariant Legacy` wybiera naszą dotychczasową linię RPM. Parametr domyślnie
pozostaje `Legacy`, także w zestawie `Extras`; nowy łuk wybieraj jawnie.

Warianty zastępują ten sam folder HudTest i nie powinny działać jednocześnie.
Po instalacji wykonaj standardowy ręczny restart iDrive. `.\Uninstall-Evo.cmd`
przywraca folder sprzed ostatniej instalacji. Renderowanie i oddawanie obszaru
nawigacji są przejęte z main; zachowanie na tym HUD wymaga sprawdzenia w aucie.
Pasywny `hud-trace-qnx` pozostaje zablokowany.