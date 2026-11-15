# Testowanie na Evo Y

Skrypty wymagaja Windows PowerShell 5.1, klienta OpenSSH oraz istniejacego
dostepu SSH do Evo. Nie odblokowuja SSH. Wszystkie przyklady uruchamiaj z
katalogu tego repozytorium. Domyslny adres SSH jednostki to `169.254.199.119`.
Opcjonalny parametr `-Head` pozwala wskazac inny adres.

Domyslnie logowanie odbywa sie **haslem root**, bez klucza uzytkownika SSH.
Instalator i deinstalator pokazuja po 5 etapow. Instalacja korzysta z dwoch
sesji SSH: jednego odczytu konfiguracji, a nastepnie jednego przeslania calej
paczki i wykonania skryptu na Evo. Deinstalacja rowniez korzysta z dwoch
sesji. Liczba polaczen nie rosnie wraz z liczba plikow. Kazde
polaczenie SSH ma numer, opis i czas wykonania; co 5 sekund oczekiwania
pojawia sie komunikat, na czym skrypt czeka. Komunikaty sa wlaczone
domyslnie, bez parametru `-Verbose`.
Haslo serwisowe jest zapisane lokalnie w `.local/evo-auth.ps1` (pomijane przez Git)
na wyrazna prosbe wlasciciela. Na tym komputerze skrypt nie pyta o nie przy uruchomieniu. Bez lokalnego pliku pyta o haslo; mozna go przygotowac wedlug `source/tools/evo-auth.example.ps1`.
Nie wpisuj hasla jako argumentu polecenia, szczegolnie gdy zawiera `&`.
Po odczytaniu ze skryptu haslo jest przechowywane jako SecureString. Pomocnik
OpenSSH pobiera je przez jednorazowy, lokalny kanal dostepny tylko dla
Twojego konta Windows. Haslo nie trafia do argumentow procesow,
potwierdzen instalacji ani zmiennych srodowiskowych. Program pomocniczy
bez hasla jest kompilowany lokalnie w `.local/ssh/`.

## Instalacja zegarow

1. Auto zaparkowane, iDrive wlaczone, komputer podlaczony przewodem ENET.
2. Sprawdz klucz hosta przy pierwszym polaczeniu przez SSH i zapisz go w
   known_hosts. Skrypty wymagaja zgodnosci tego klucza; nie akceptuja go same.
3. Uruchom `Install-Evo.cmd` (takze przez dwuklik). Potwierdz, ze auto stoi. Domyslnie instalowane sa **BoostDeck i ShiftDeck** (zestaw Gauges).
4. Po komunikacie o weryfikacji zrestartuj iDrive recznie, przytrzymujac
   pokretlo glosnosci. Sprawdz **My Vehicle -> BoostDeck / ShiftDeck**, wejscie i wyjscie
   z ekranu oraz kolejne uruchomienie iDrive.
5. Uruchom `Uninstall-Evo.cmd`. Skrypt wycofa ostatnia
   udana instalacje dla tego adresu. Nastepnie zrestartuj iDrive recznie.

Opcje z dzialajacego polaczenia (`HostKeyAlgorithms=+ssh-rsa` oraz
`MACs=+hmac-sha1`) sa wlaczone domyslnie. Parametr `-Legacy` pozostaje
obslugiwany dla zgodnosci, ale nie jest juz potrzebny:

```powershell
.\Install-Evo.cmd
.\Uninstall-Evo.cmd
```

Opcjonalnie dostepny jest zestaw **Core**: ShiftDeck oraz
EngineCodes z odczytem i kasowaniem bledow DME. Kasowanie ma osobny ekran
potwierdzenia i nie wymaga MHD ani Bootmod3. Instalator nie wydaje zadnych
polecen do ECU; dopiero wybor funkcji w menu uruchamia odpowiedni worker.

```powershell
.\Install-Evo.cmd -AppSet Core -Legacy
.\Uninstall-Evo.cmd -Legacy
```

`-AppSet All` dodaje BoostDeck. Jego kanaly i przeliczenia pochodza z B58,
sa **niezweryfikowane na N20B20** i wymagaja porownania z niezaleznym
odczytem. Obroty i predkosc ShiftDeck korzystaja ze standardowego OBD,
ale ich dzialanie na tym aucie rowniez wymaga sprawdzenia. Bramke w tym
buildzie konfiguruje sie plikiem `gateway-ip.txt` przy kazdym workerze.
Nowy instalator domyslnie wpisuje `169.254.20.21`, wskazane przez wlasciciela
i potwierdzone odczytem OBD z Evo (RPM, predkosc i temperatura).
Poprzednie biblioteki uzywaly stalego `169.254.21.3`.

```powershell
.\Install-Evo.cmd -AppSet All -Gateway 169.254.20.21
```

`Core` instaluje tylko ShiftDeck i EngineCodes. `All` dodaje BoostDeck.
Parametr `-Gateway` zmienia konfiguracje bez rekompilacji. Po aktualizacji
aplikacji wykonaj reczny restart iDrive, aby zaladowalo nowe biblioteki.

Aktualny BoostDeck dla seryjnego DME pokazuje **OIL TEMP** zamiast Active E.
Temperatura oleju i coolant korzystaja ze standardowych odczytow OBD,
a rail pressure ma fallback OBD po odrzuceniu identyfikatora BMW. Te trzy
odczyty potwierdzono na N20. Jednostki temperatury zmieniaja sie w OPTIONS
razem z pozostalymi wskazaniami. Aktualizacja samego BoostDeck:

```powershell
.\Install-Evo.cmd -Apps BoostDeck
```

Nastepnie zrestartuj iDrive recznie. Nie podmieniaj samego workera: nowy
format NBG5 chroni przed pomyleniem temperatury oleju z dawnym procentem etanolu.

## Brak danych diagnostycznych

Uruchom jeden ograniczony test z poziomu Evo:

```powershell
.\Test-Evo-Diagnostics.cmd
```

Auto powinno stac z wlaczonym zaplonem. Wyjdz z aplikacji zegarow i zamknij
inne testery diagnostyczne. Test sprawdza najpierw `169.254.20.21`, potem
stary adres oraz trzy adresy widoczne w wewnetrznej sieci Evo. Wykonuje tylko
odczyty OBD `01 0C`, `01 0D`, `01 05` (RPM, predkosc, temperatura).
Nie kasuje bledow, nie zmienia sesji ECU, nie instaluje wtyczek i nie restartuje.
Przesyla tylko tymczasowe narzedzie do osobnego katalogu roboczego.

Samo `TCP CONNECTED` nie dowodzi komunikacji z DME. Szukaj pozytywnej
odpowiedzi, np. `READ RPM ... response=410C...`. `Diagnostics busy` oznacza,
ze diagnostyke zajmuje inny worker. Pelny wynik zapisuje sie w
`.local/diagnostics/`. Brak dostepu SSH do Evo nie rozstrzyga dostepnosci
samej bramki diagnostycznej.

Dla zestawow Core/All instalator odczytuje geometrie ekranu i wymaga
1280x480. Jesli LayerManager nie odpowiada, mozna podac **sprawdzone**
wymiary `-DisplayWidth 1280 -DisplayHeight 480`. Samo 1280 w poziomie
nie potwierdza wysokosci.

## Podglad i wybor aplikacji

`-WhatIf` sprawdza lokalne pliki i pokazuje zamiar operacji **bez SSH**:

```powershell
.\Install-Evo.cmd -AppSet Core -WhatIf
```

Zaawansowany wybor aplikacji dziala bezposrednio w PowerShell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& './source/tools/install-dashboard.ps1' -Apps ShiftDeck,EngineCodes -Legacy"
```

Parametr `-Parked` zastepuje pytanie o zaparkowane auto. `-IdentityFile`
pozwala wskazac klucz prywatny SSH. Hasla ani klucze nie sa zapisywane w
potwierdzeniach instalacji. Polityka wykonywania skryptow jest pomijana
tylko dla uruchomionego procesu PowerShell.
Skrypty korzystaja z domyslnego hasla.
Parametr PowerShell `-Password` (SecureString) pozwala je nadpisac.
Podanie `-IdentityFile` wybiera zamiast tego logowanie
kluczem. Wymagany jest OpenSSH obslugujacy SSH_ASKPASS_REQUIRE=force
(Win32-OpenSSH 8.6 lub nowszy).

Sam odczyt wersji firmware, bez instalacji i restartu:

```powershell
.\Test-Evo-SSH.cmd
```

Oczekiwany komunikat: `SSH OK: NBTevo_Y24073J`. Przy bledzie skrypt pokazuje
komunikat klienta SSH, np. przyczyne odmowy logowania lub problem z kluczem hosta.
Ustawienia polaczenia odpowiadaja dzialajacemu deploy-evo.ps1 z projektu
Android Auto: timeout 15 sekund, keepalive 10 sekund, haslo lub
keyboard-interactive oraz DISPLAY dla pomocnika askpass.

## Kopie i wycofanie

Instalator sprawdza dokladny firmware i hashe obu frameworkow Y24073J,
kolizje nazw i connector ID, lokalne sumy kontrolne oraz SHA256 plikow
zapisanych na Evo. Pomocnik wykonuje weryfikacje na jednostce, bez osobnego
polaczenia po kazdy plik. Nie wymaga tar, printf, rmdir ani openssl w powloce.
Przed wlaczeniem aplikacji foldery sa
przygotowywane poza Plugins. Poprzednie wersje sa przenoszone do kopii.
Blokada nie pozwala uruchamiac dwoch operacji tych skryptow jednoczesnie.

Potwierdzenia instalacji: `.local/deployments/<RunId>.json` na komputerze.
Pliki i kopie na jednostce:

```text
/var/opt/hmi/data/ID5/CodexAppTests/<RunId>/
  backup/    poprzednie aplikacje
  removed/   wycofane aplikacje testowe
  failed/    aplikacje odlozone przy wycofywaniu nieudanej instalacji
  stage/     przygotowywane pliki
  receipt.json
  STATUS
```

Mozna wycofac konkretny zestaw, takze bez lokalnego potwierdzenia:

```powershell
.\Uninstall-Evo.cmd -RunId 20261115T120000Z-0123456789ab -Legacy
```

Jawny `-RunId` przydaje sie rowniez, jesli adres ENET jednostki sie zmienil.
Skrypt odczytuje wtedy potwierdzenie tego zestawu bezposrednio z jednostki.

Deinstalator przenosi aplikacje do `removed/` i przywraca poprzednie
foldery. Nie usuwa ich rekurencyjnie. Odmawia wycofania, jesli od instalacji
zmienily sie pliki aplikacji; chroni to pozniejsze zmiany i nowsze wersje.
Kolejne instalacje tej samej aplikacji wycofuj od najnowszej.

Po bledzie przenoszenia folderow skrypt probuje wycofac cala transakcje.
Utrata zasilania lub lacznosci moze pozostawic operacje nieukonczona.
Wtedy zachowaj komunikat z RunId i sprawdz STATUS oraz foldery kopii przed
kolejna operacja lub restartem. Nie usuwaj w ciemno blokady
`/var/opt/hmi/data/ID5/CodexAppTests.lock`. Nie restartuj iDrive, dopoki
stan Plugins nie jest wyjasniony.

Skrypty nie restartuja jednostki i nie modyfikuja firmware, kodowania ani
bibliotek BMW. Ich transakcje przetestowano na sztucznym systemie plikow
Linux, z wymuszonymi bledami instalacji i deinstalacji. Zachowanie na QNX
i zgodnosc bibliotek systemowych pozostaja do sprawdzenia na jednostce.
Przeslanie paczki, odbior i weryfikacje SHA256 na QNX sprawdzono rzeczywiscie
narzedziem diagnostycznym: cala sesja z piecioma adresami trwala okolo 30 s.
Pelna instalacja nowej wersji aplikacji nadal wymaga testu wlasciciela.

## Przebudowanie plikow lokalnych

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File source/tools/build-dashboard.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File source/tools/check-dashboard.ps1
```

Skrypty instalacyjne korzystaja z `source/native/out/dashboard/candidate`
i pomocnika `source/native/out/dashboard/gaugedeck-fileops`.

## Usuniete aplikacje testowe

AbiSmoke i CdsProbe zakonczyly testy i nie sa juz budowane ani instalowane. Przy kolejnym polaczeniu z autem uruchom `.\Remove-Evo-Tests.cmd`, a potem zrestartuj iDrive recznie. Skrypt korzysta z ich osobnych starych potwierdzen instalacji; nie wycofuje instalacji zegarow.

## Zegary CDS - aktualna paczka

`.\Install-Evo.cmd -AppSet Gauges` aktualizuje BoostDeck i ShiftDeck. Oba zegary korzystaja tylko z CDS; BoostDeck ma nowy zestaw 9 pol. Reczny wybor jednostek usuniety: predkosc, temperatury i paliwo z ustawien Evo, moment jawnie w Nm. Restart reczny. Szczegoly: source/docs/CDS-GAUGES.md. Engine Fault Codes zachowuje osobny transport diagnostyczny. Nowe dodatkowe kanaly wymagaja sprawdzenia w aucie.

## Aktualny BoostDeck MOST

`.\Install-Evo.cmd -Apps BoostDeck` instaluje BoostDeck z odczytami DME przez MOST; ShiftDeck pozostaje na CDS. Restart reczny. Powracaja BOOST, AFR, TIMING, INTAKE AIR, RAIL PRESSURE i OIL PRESSURE. Temperatury silnika/oleju oraz RPM z CDS. RPM zastepuje temperature skrzyni. Cisnienia jawnie w bar, temperatury wedlug ustawien Evo. Brak danych = kreski. Po instalacji sprawdzic ponowne otwarcie i pelne uspienie/wybudzenie bez OBD. Szczegoly: source/docs/BOOSTDECK-MOST.md. Test tymczasowy: Test-Evo-MOST.cmd. Dotychczasowy test MOST odbyl sie z kablem do SSH; calej nowej aplikacji jeszcze nie sprawdzono w aucie.

## HUD i xDrive - przygotowane offline

Instaluj osobno: `.\Install-Evo.cmd -Apps HudTest` lub `.\Install-Evo.cmd -Apps XDriveControl`. Obie razem: `.\Install-Evo.cmd -AppSet Extras`. Po instalacji restart reczny. HUD jest domyslnie wylaczony; pierwszy wariant ma tekstowe RPM i boost w bar. Profil wyswietlania wymaga proby na postoju. xDrive zachowuje HSFZ; najpierw tylko Read transfer case mode. Przelaczanie napedu wymaga wylaczonego silnika, Park, postoju, hamulca i zgodnych odpowiedzi modulow; profilu tej F30 nie potwierdzono. Deinstalacja menu nie przywraca stanu napedu. `Test-Evo-Exhaust.cmd` tylko odczytuje identyfikacje stock DME i kandydat 60FD, nie otwiera klapy. Szczegoly: source/docs/HUD-XDRIVE-EXHAUST.md.
