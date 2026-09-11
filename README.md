# Karadag Beat

Gross Beat tarzi, tempoya kilitli zaman ve ses manipulasyonu eklentisi. VST3 (FL Studio icin)
ve bagimsiz uygulama olarak derlenir.

![Arayuz](docs/arayuz.png)

---

## Ne yapiyor

Eklenti gelen sesi surekli bir halka buffer'da tutar ve pattern boyunca (1, 2 ya da 4 bar;
varsayilan 2) uzerine uc zarf uygular:

**Time zarfi** - okuma kafasinin ne kadar geriden okudugunu belirler.
Dikey eksen 0 (canli, en ust) ile bir pattern boyu geride (en alt) arasidir. Isin puf noktasi sudur:

| zarfin egimi | okuma hizi | duyulan |
|---|---|---|
| 0 (duz cizgi) | 1.0 | normal hiz, sabit gecikme |
| +0.5 | 0.5 | yarim hiz, bir oktav asagi |
| +1 | 0 | okuma kafasi yerinde sayar |
| -1 | 2.0 | cift hiz, bir oktav yukari |
| >+1 | negatif | ses geri sariyor |

Yani **stutter, freeze, half-time, tape stop, scratch** ayri ayri efektler degil; hepsi ayni
zarfin farkli sekilleri. Basamakli (step) noktalar sabit gecikme verdigi icin perdeyi bozmadan
dilim tekrarlar; egimli noktalar hizi ve dolayisiyla perdeyi degistirir.

**Volume zarfi** - dogrudan gain carpani. Gate, sidechain pump, tremolo bundan cikar.

**Filter zarfi** - islenmis sesin uzerindeki bir filtrenin kesim frekansi. Ust = acik,
alt = kapali. **LP** (low-pass) kapandikca sesi karartir (20 kHz -> 40 Hz), **HP**
(high-pass) inceltir (20 Hz -> 12 kHz); aralik logaritmik. **RESO** rezonansi ayarlar.
Filtre gain'den sonra calisir, yani rezonansli bir filtre gate kapandiktan sonra da kisa bir
kuyruk birakir. Lane tamamen acikken filtre cikisa hic karismaz - bypass bit-bit seffaf.
Gross Beat'te olmayan bir ozellik: filtre gate'leri, sweep'ler, "telefon" gecisleri.

21 fabrika pattern'inin hepsi tek bakista (fabrika pattern'leri time ve volume kullanir;
filtre lane'i senin cizimlerin icin):

![Fabrika pattern'leri](docs/presets.png)

Tik sesine karsi iki koruma var:

- **Time tarafi:** zarfta ani sicrama oldugunda iki okuma kafasi arasinda capraz gecis yapilir
  (varsayilan 4 ms, time lane'indeki **SMOOTH** ile 1-80 ms).
- **Volume tarafi:** gain bir slew limiter'dan gecer - yumusak egrilere (pump, fade) dokunmaz,
  yalnizca izin verilenden hizli sicramalari (gate kenarlari, VOLUME dugmesi, MIDI ile pattern
  degisimi) rampaya cevirir (varsayilan 2 ms, volume lane'indeki **SMOOTH** ile 0.5-80 ms).
  Mix knob'u da ornek basina yumusatilir.
- **Filter tarafi:** kesim frekansi da ayni sekilde slew limiter'dan gecer (varsayilan 5 ms,
  filter lane'indeki **SMOOTH** ile 0.5-80 ms). Uzun bir sure, basamakli bir filtre zarfini
  yumusak sweep'lere cevirir.

Kesirli okuma konumlarinda 32 noktali, Kaiser pencereli bir **sinc** interpolasyonu kullanilir.
Okuma kafasi normalden hizli ilerlediginde (Double Time, Scratch, Reverse) cekirdegin kesim
frekansi hiza gore daraltilir; boylece Nyquist'i asacak frekanslar geri katlanmak yerine
suzulur. Normal hizda cekirdek saf bir darbe oldugu icin bypass bit-bit seffaf kalir.
Olcum sonuclari asagida.

---

## Kurulum

```powershell
.\build.ps1
```

Derler, DSP testlerini calistirir, testler gecerse VST3'u kurar.
Testler kalirsa kurulum yapilmaz.

Secenekler: `-SkipTests` (testleri atla), `-Configure` (CMake'i bastan yapilandir),
`-Validate` (kurmadan once pluginval ile dogrula; `tools\pluginval\pluginval.exe` gerekir),
`-Installer` (Inno Setup 6 ile `build\installer\KaradagBeat-<surum>-Setup.exe` uretir).

### Baskasina vermek: kurulum dosyasi

`installer\KaradagBeat.iss` tek bir `Setup.exe` uretir: VST3'u sistem klasorune, istenirse
bagimsiz uygulamayi Program Files'a kurar, Baslat menusune kisayol ve kaldirici ekler.
Kaldirirken kaydedilmis pattern'lere dokunmaz.

### GitHub Actions

Her `main` push'unda `.github/workflows/build.yml` Windows'ta derler, iki test takimini
calistirir, eklentiyi **pluginval** (strictness 10) ile dogrular ve kurulum dosyasini uretir.
Hazir `Setup.exe` ve VST3 zip'i GitHub'da **Actions** sekmesinde, calismanin **Artifacts**
bolumunden indirilir. JUCE runner'da yoksa CMake ayni surumu (8.0.10) GitHub'dan ceker.
Private depoda Windows dakikalari aylik ucretsiz kotadan iki kat duser (bir calisma ~20-30 dk).

### Eklenti nereye kuruluyor

```
C:\Program Files\Common Files\VST3\Karadag Beat.vst3
```

FL Studio pratikte yalnizca bu klasoru tariyor; kullanici klasorune
(`%LOCALAPPDATA%\Programs\Common\VST3`) kurulan eklentiyi arama yollarina elle eklemedigin
surece gormuyor. Bu yuzden kurulum dogrudan sistem klasorune yapiliyor.

Klasor yonetici izni istedigi icin script bir **UAC penceresi** acar - onaylaman yeterli.
PowerShell'i zaten yonetici olarak acmissan pencere hic cikmaz.

Kurulumdan sonra FL'nin yeni eklentiyi gormesi icin bir kez taratmak gerekir:

**Options > Manage plugins > Find more plugins**

Tarama bitince eklenti **Effects > VST3** altinda "Karadag Beat" olarak gorunur.

---

## FL Studio'da kullanim

Eklentiyi bir **efekt (FX) slotuna** ekle - enstruman degil, ses uzerinde calisir.

### MIDI ile pattern tetikleme

Gross Beat'in en cok kullanilan ozelligi: pattern'leri piano roll'dan tetiklemek.

- **C4'ten itibaren 48 nota** slotlari sirayla secer (C4 = slot 1, C#4 = slot 2, ... B7 = slot 48).
- Nota basili oldugu surece o pattern calisir, birakinca eklenti secili pattern'e doner.
- Ayni anda birden fazla nota basiliysa slot araligindaki en tiz nota kazanir.
- Calan slotun numarasi MIDI dugmesinin uzerinde gorunur ("MIDI 12"), editor de o slotu
  gosterir. Nota birakilinca senin cizimin aynen geri gelir - slota kaydetmemis olsan bile.
  Slot gosterilirken yaptigin degisiklikler yalnizca o an calana uygulanir, cizimine dokunmaz.
- Alt bardaki **MIDI** dugmesiyle kapatilabilir.

**LATCH** acikken davranis degisir: bir nota pattern'ini acar ve nota birakilsa da acik
kalir; ayni nota tekrar calininca kapanir, baska bir nota dogrudan o pattern'e gecer.
Uzun freeze'ler icin piano roll'a uzun nota cizmek gerekmez.

**RETRIG** acikken her nota pattern'ini **bastan** baslatir. Normalde pattern host'un
konumuna kilitlidir: bir vurusun ortasina basilan nota, pattern'e de ortasindan girer. Retrigger
ile tape stop, scratch ya da backspin tam notanin basladigi sample'da baslar - tek bir vurusa
ya da bir fill'e efekt atmanin yolu. Nota birakilinca pattern host'un o anki konumuna geri
kilitlenir. Alttaki bir notaya basmak (kazanan degismediyse) calani yeniden baslatmaz.

Pattern degisimi sample dogrulugunda: blok, MIDI olayinin dustugu noktadan bolunerek
isleniyor, yani vurusa konan bir nota pattern'i tam o vurusta degistirir (buffer boyutundan
bagimsiz).

FL'de efekt eklentisine MIDI gonderebilmek icin eklenti penceresinin sol ust kosesindeki
disli menusunden MIDI giris portunu ayarlaman ve ayni portu bir MIDI Out kanalina vermen gerekir.

### MIDI kurmadan: automation clip

MIDI yonlendirmesiyle ugrasmak istemezsen, **Pattern** zaten otomasyona acik bir parametre.
FL'de eklentide bir pattern secip `Tools > Last tweaked > Create automation clip` ile bir
automation clip olustur; clip'teki her kademe bir slota karsilik gelir. Ayni sey
**Length**, **Mix**, **Swing**, **Filter Resonance**, **Chain** ve smooth sureleri gibi diger
parametreler icin de gecerli.

### Slot zinciri (CHAIN)

Otomasyon klibi cizmeden varyasyon: zincir, her pattern turunda siradaki adimi calar.
Ornegin 2 bar'lik pattern ve 4 adim = 8 bar'lik bir dongu:

| adim | slot |
|---|---|
| 1 | Drawing (editordeki cizim) |
| 2 | Drawing |
| 3 | Repeat 1/8 |
| 4 | Tape Stop |

![Zincir paneli](docs/zincir.png)

**CHAIN** zinciri acip kapatir, yanindaki **...** paneli acar: 1-8 adim, her adim bir slot ya
da **Drawing**. Calan adim hem panelde hem dugmenin uzerinde gorunur ("CHAIN 3/4"). Adim
degisimi turun tam basladigi sample'da olur; blok ortasina dussa bile.

Zincir host'un konumunu takip eder: projeyi 5. bar'dan baslatirsan zincir de o bar'a denk
gelen adimdan baslar. Transport durukken kendi turlarini sayar. MIDI notasi zinciri ezer;
nota birakilinca zincir kaldigi yerden devam eder. Zincir (adimlar ve uzunluk) projeyle
birlikte kaydedilir.

### Swing

Alt bardaki **SWING** her ikinci 1/16'ligi geriye iter: %50 duz, %66 triole yakin, %75 en sert.
Vuruslar ve 1/8'likler yerinden oynamaz. Butun lane'lere uygulanir - gate'ler, stutter
basamaklari, filtre adimlari hep birlikte swing'lenir. Cift tik %50'ye dondurur.

Time lane'inde swing yalnizca noktalari kaydirmaz: bir repeat adimi, swing'le kaymis olsa bile
**kaynak adimin tam basindan** ve **normal hizda** calar. (Saf bir zaman kaydirmasi ya
transient'in basini keser ya da perdeyi oynatirdi; test ikisini de olcuyor.)

Editor duz izgarayi gosterir; calan kafa ve arkadaki dalga formu swing'e gore eslenir, yani
kafa bir noktaya geldiginde duydugun o noktadir.

---

## Arayuz

Arayuz Ingilizce.

- **Ust bar:** pattern slotu secici (FACTORY / YOUR PATTERNS basliklariyla), **SAVE**,
  **FILE**, **UNDO** / **REDO**, snap bolumu (1/4 ... 1/32, uclemeler dahil),
  **pattern uzunlugu**, palet dugmesi, mix knob'u.
- **Alt bar:** **MIDI**, **LATCH**, **RETRIG**, **CHAIN** ve zincir paneli (**...**), **SWING**.

Uc zarf paneli: **time**, **volume**, **filter**. `TIME` / `VOLUME` / `FILTER` dugmeleri
ilgili zarfi devre disi birakir. Her panelin baslik satirinin saginda o lane'e ait araclar var:
**SMOOTH** (yukarida anlatilan yumusatma suresi), `<` `>` lane'i bir izgara adimi kaydirir
(groove icin), **DRAW** cizim modunu acar. Filter panelinde ayrica **LP / HP** secici ve
**RESO** var; dikey olcek o yuksekligin kesim frekansini Hz olarak yazar.

Lane'lerin arkasinda soluk bir **dalga formu** var: gelen sesin pattern boyunca nerede yogun
oldugunu gosterir (dB olcekli, -48 dBFS'e kadar). Gate'leri kick'lere hizalamak ya da neyi
stutter'ladigini gormek icin.

| hareket | sonuc |
|---|---|
| sol tik (bos alan) | yeni nokta ekler ve suruklemeye baslar |
| sol tik (nokta) | noktayi tasir |
| sag tik (nokta) | noktayi siler |
| sag tik (bos alan) | menu: Reset / Flip vertically / Copy / Paste |
| cift tik (nokta) | basamak / egri modunu degistirir |
| segment ortasindaki halkayi surukle | egriyi buker - egrinin ortasi fareyi birebir takip eder |
| cift tik / sag tik (halka) | egriyi duz cizgiye dondurur |
| fare tekerlegi (segment uzerinde) | segmentin egrisini buker |
| **DRAW** acikken ya da **Alt** + surukle | gecilen her izgara hucresine o yukseklikte bir basamak boyar |
| Shift basili | snap kapali, serbest surukleme |

**Cizim modu** Gross Beat'teki gibi pattern'i fareyle "boyamak" icin: volume lane'inde gate,
time lane'inde stutter basamaklari. Yalnizca boyanan hucreler degisir; hucrenin oncesindeki ve
sonrasindaki sekil (rampa, egri) aynen korunur. Hizli surukleseniz de arada hucre atlanmaz.

**Copy / Paste** panosu uc lane arasinda ortak - volume'daki bir gate'i kopyalayip filter'a
yapistirmak, ayni ritimde bir filtre gate'i verir.

**UNDO** / **REDO** dugmeleri zarf duzenlemelerini geri alir (64 adima kadar). Bir surukleme
hareketi tek bir adim sayilir. Ctrl+Z / Ctrl+Shift+Z de calisir, ama host klavyeyi kendisi
yakalayabildigi icin (FL'de Ctrl+Z projeyi geri alir) dugmeler daha guvenilir yoldur.

Beyaz dikey cizgi calan kafadir; host'un transport'una kilitlidir, projeyi
durdurup baslattiginda pattern hep ayni yerden devam eder.

### Pattern uzunlugu

Ust bardaki **1 bar / 2 bars / 4 bars** secici butun izgaranin kac bar surdugunu belirler.
Gross Beat 2 bar'a sabittir; burada 1 bar kisa stutter'lar, 4 bar uzun freeze'ler icin
kullanilabiliyor.

Snap araligi da buna gore olceklenir: 1/16 secildiginde izgara her zaman gercek 1/16'lik
notalara denk gelir, pattern kac bar olursa olsun. Dikey olcek de bar cinsinden etiketlenir.

### Renk paleti

Ust bardaki uc noktali kucuk dugme paleti degistirir (noktalar o paletin time, volume ve
filter renkleri):

| palet | karakter |
|---|---|
| **Toxic Bloom** | patlican moru zemin, asit lime + sicak magenta (varsayilan) |
| **Ember** | sicak kizil-kahve zemin, mint + amber - cogu eklentinin tam tersi |
| **Ultraviolet** | gece mavisi zemin, elektrik mor + asit sari |
| **Chlorine** | koyu petrol zemin, klor mavisi + neon turuncu |
| **Blood Orange** | koyu bordo zemin, soluk yesil + kan portakali |
| **Moss** | yosun zemin, kirli lime + toz pembe |
| **Bruise** | morarmis mor zemin, soluk lila + asit sari |
| **Concrete** | notr beton grisi, sari + murekkep mavisi - brutalist |

Secim `%APPDATA%\Karadag\KaradagBeat\settings.xml` dosyasina yazilir, tum projelerde ayni kalir.

Yeni palet eklemek icin `Source/Theme.cpp` icindeki listeye bir kayit eklemek yeterli -
arayuzdeki hicbir renk baska bir dosyada tanimli degil.

### Kendi pattern'lerini kaydetmek

48 slot var: ilk 21'i fabrika pattern'leri (salt okunur), kalan 27'si senin.

**SAVE** dugmesi cizili zarflari bir kullanici slotuna yazar. Acilan pencerede hem ad hem
**hedef slot** secilebiliyor, yani bir pattern'i baska bir slota kopyalamak icin onu yukleyip
farkli bir slota kaydetmek yeterli. Varsayilan hedef: secili slot senin bir slotunsa o,
degilse ilk bos slot.

Kayitli bir pattern'in uzerine yazmak geri alinamadigi icin, **baska** bir dolu slota
kaydederken onay istenir. (Kendi slotunu yukleyip ayni slota geri kaydetmek bilincli bir
hareket sayilir, onay cikmaz.) Onay penceresinde **Overwrite**'in klavye kisayolu yok -
iki kez Enter'a basarak kazara uzerine yazilamasin diye; Escape vazgecer.

Kaydedilen pattern'ler su dosyada tutulur ve tum projelerde kullanilabilir. Dosyaya mutlak slot
numarasi degil, kacinci kullanici slotu oldugu yazilir; boylece ileride fabrika pattern'i
eklendiginde kayitlarin kaymaz:

```
%APPDATA%\Karadag\KaradagBeat\user_patterns.xml
```

Projeye ozel cizimler ayrica proje dosyasina da kaydedilir, yani slota kaydetmeden
cizdigin bir zarf da proje tekrar acildiginda yerinde olur. Eklenti penceresini kapatip
acmak da cizime dokunmaz.

### Pattern paylasmak

**FILE** menusu cizili pattern'i bir `.kbeat` dosyasina yazar ya da bir dosyadan yukler.
Dosyada uc zarf, pattern uzunlugu ve dosyaya verdigin ad bulunur (filtresiz eski dosyalar da
acilir, filtre acik gelir). Varsayilan klasor
`Belgeler\Karadag Beat Patterns`. Yuklenen pattern cizim alanina gelir (geri alinabilir);
kalici bir slotta tutmak icin ardindan **SAVE** ile kaydet. Bozuk ya da baska bir dosya
reddedilir ve mevcut cizim bozulmaz.

---

## Gelistirme

### Dosyalar

| dosya | is |
|---|---|
| `Source/Envelope.*` | zarf veri yapisi, segment aramasi, tension egrileri, hucre boyama, kaydirma, kalici hale getirme |
| `Source/GrossEngine.*` | halka buffer, kesirli okuma kafasi, hiza bagli anti-alias'li sinc interpolasyon, capraz gecis, gain/mix yumusatma, dalga formu tepeleri |
| `Source/Presets.*` | fabrika pattern'leri ve onlari ureten yardimcilar |
| `Source/Swing.*` | swing zaman bukmesi ve zarflarin swing'lenmesi |
| `Source/FilterMap.h` | filtre lane degeri -> kesim frekansi (DSP ve olcek etiketleri ortak) |
| `Source/PluginProcessor.*` | parametreler, host tempo senkronu, MIDI tetikleme ve retrigger, zincir, taban / gecici pattern kaynaklari, thread guvenligi |
| `Source/Theme.*` | renk paletleri - arayuzdeki tek renk kaynagi |
| `Source/PluginEditor.*` | ana pencere, imza, yerlesim, zincir paneli, geri alma yigini |
| `Source/EnvelopeEditor.*` | duzenlenebilir zarf cizim alani |
| `Tests/DspTest.cpp` | DSP dogrulama (host gerekmez) |
| `Tests/StateTest.cpp` | proje kaydet/ac turu, pattern uzunlugu, MIDI tetikleme, latch, retrigger ve sample dogrulugu, zincir, cizimin korunmasi, disa/ice aktarma, bozuk durum, mono/stereo |
| `Tests/AliasTest.cpp` | hizli calmada bozulma olcumu (FFT) |
| `Tests/PresetRender.cpp` | tum pattern'leri tek PNG'ye cizer |
| `Tests/EditorRender.cpp` | eklenti penceresini ekran disinda PNG'ye cizer (arayuz duzeni kontrolu) |
| `installer/KaradagBeat.iss` | Inno Setup kurulum betigi |
| `.github/workflows/build.yml` | GitHub Actions: her push'ta derleme, testler, pluginval, kurulum dosyasi |

### Testler

```powershell
.\build\DspTest_artefacts\Release\DspTest.exe
```

Bypass seffafligini, half/quarter time'in gercekten dogru perdeyi verdigini, stutter'in perdeyi
bozmadigini, gate'in tam susturdugunu, dort ayri ornekleme frekansi ve tempoda ayni sonucun
ciktigini, blok boyutunun cikisi hic degistirmedigini, hicbir pattern'de ve VOLUME / mix
degisimlerinde tasma veya tik olmadigini olcer. Hizli zarf aramasini eski algoritmaya karsi,
kaydirma ve hucre boyamayi da ayri ayri dogrular; SMOOTH ayarlarinin istenen gecis surelerini
verdigini (orn. 20 ms ayar -> %90'dan %10'a 16.00 ms) ve dalga formu dilimlerinin dogru yere
dustugunu olcer; 128 noktali zarflarla islem hizini da yazar.

Swing icin: gercek <-> pattern zamani donusumunun tersinir oldugunu, swing'li bir repeat
adiminin kaynak adimin basindan (0.0 sample kayma) ve normal hizda calindigini, gate
kenarlarinin swing'li konuma dustugunu olcer. Filtre icin: acik lane'in bit-bit seffaf
oldugunu, LP'nin 189 Hz kesimde 5 kHz'i -57 dB, HP'nin 2.4 kHz kesimde 50 Hz'i -67 dB
bastirdigini, basamakli filtre zarfinin tiksiz, tam rezonansta taramanin kararli oldugunu
dogrular.

Tik testleri kasten 437 Hz ile yapiliyor: 300 Hz gibi "yuvarlak" bir frekans 120 BPM'de
adim sinirlarina tam hizalanip gate kenarlarini sinusun sifir gecisine denk getiriyor ve
kliki tamamen gizliyordu - volume tarafindaki tik boyle gozden kacmisti.

```powershell
.\build\StateTest_artefacts\Release\StateTest.exe
```

Projeyi kaydedip acmanin zarflari ve parametreleri aynen geri getirdigini, pattern uzunlugunun
gercekten DSP'ye ulastigini, blogun ortasina dusen bir notanin pattern'i tam o sample'da
degistirdigini (oncesindeki sample'lara hic dokunmadan), bozuk bir durum blogunun eklentiyi
cokertmedigini ve mono/stereo kanal duzenlerinin calistigini dogrular.

Sahte bir host transport'u ile: retrigger'in host nerede olursa olsun pattern'i notanin
sample'inda bastan baslattigini ve nota birakilinca host fazina dondugunu, zincirin adimlari
tam tur sinirinda degistirdigini (blok ortasinda bile) ve transport durukken de ilerledigini,
cizimin MIDI notasindan, host'un yeniden hazirlamasindan ve **eklenti penceresinin
acilmasindan** sonra yerinde kaldigini olcer.

```powershell
.\build\AliasTest_artefacts\Release\AliasTest.exe
```

Sabit hizli bir zarf kurup saf sinus gecirir, cikisi FFT ile inceleyip beklenen ton disindaki
butun enerjiyi olcer. Hiz 1.0 satiri referans tabandir (-120 dB); diger hizlarin ondan ne kadar
uzaklastigi bozulmanin gercek boyutunu verir. Hermite'ten sinc'e gecisin etkisi (THD+N):

| giris, hiz | Hermite | sinc |
|---|---|---|
| 8 kHz, 1.25x | -36 dB | -88 dB |
| 11 kHz, 1.25x | -26 dB | -88 dB |
| 14 kHz, 1.25x | -19 dB | -88 dB |
| 14 kHz, 1.5x | -19 dB | -84 dB |

Nyquist'i asan durumlarda beklenen ton temsil edilemez; test geri katlanan bileseni mutlak
seviyesiyle (dBFS, tam olcekli sinus = 0) raporlar. Hiza bagli kesim frekansinin etkisi:

| giris, hiz | katlandigi yer | onceden | simdi |
|---|---|---|---|
| 20 kHz, 2x | 8 kHz | 0 dBFS | -105 dBFS |
| 17 kHz, 2x | 14 kHz | 0 dBFS | -82 dBFS |
| 20 kHz, 1.5x | 18 kHz | 0 dBFS | -84 dBFS |
| 14 kHz, 2x | 20 kHz | 0 dBFS | -24 dBFS |
| 17 kHz, 1.5x | 22.5 kHz | 0 dBFS | -13 dBFS |

Duyulabilir bolgeye dusen alias'lar -80 dBFS'in altinda. Son iki satir Nyquist sinirinin
hemen ustunden katlanip 20 kHz ve ustune dusenler; oradaki zayiflama cekirdegin gecis
bandindan geliyor. Gecmesi gereken tonlar neredeyse hic kisilmiyor (8 kHz @ 2x -> 16 kHz:
0.0 dB; en kotu durum 11 kHz @ 2x -> 22 kHz: -2.3 dB).

```powershell
.\build\PresetRender_artefacts\Release\PresetRender.exe
```

Bulundugun klasore `presets.png` cizer - tum pattern'lerin sekli tek bakista gorunur.

### Thread guvenligi

GUI ve ses thread'i ayri zarf kopyalari kullanir. GUI bir degisiklik yaptiginda
`publishEnvelopes()` ile kilit altindaki bir ara kutuya yazar; ses thread'i her blokta
`ScopedTryLock` ile bakar, alamazsa bir sonraki bloga birakir. Zarf vektorleri onceden
`reserve` edildigi icin ses thread'inde bellek ayirma olmaz.

Ses thread'i iki kaynak tutar: **taban** (editordeki cizim - GUI yayini ya da preset
parametresi) ve **gecici** (MIDI notasi ya da zincir adiminin caldigi slot). Gecici kaynak
bitince taban aynen geri gelir; host `prepareToPlay`'i yeniden cagirsa da. Preset yalnizca
**secim degistiginde** tabana yuklenir; boylece elle cizilmis bir zarf sonraki blokta
preset tarafindan ezilmez. Swing, zarflar degistiginde bir kez uygulanir, ornek basina ek
is yoktur.

---

## Bilinen sinirlar

- Halka buffer 24 saniyeye gore ayrilir; 40 BPM altinda 4 bar'lik pattern bu sureyi
  asacagi icin gecikme sinirlanir.
- Hizli calmada Nyquist sinirinin hemen ustunden katlanan bilesenler (20 kHz ve ustune
  dusenler) yalnizca 13-24 dB bastiriliyor - yukaridaki tabloya bak. Duyma sinirinin ustunde
  kaldiklari icin pratikte sorun degil; daha iyisi daha uzun bir cekirdek ister.
- Gecikme sifirdan yeni kalkarken (ilk 16 sample) sinc cekirdegi yazma kafasini asacagi icin
  o kisa bolumde Hermite kullaniliyor.
- Hiz tahmini ~1 ms yumusatiliyor; hizin ani degistigi bir segment gecisinde kesim frekansi
  birkac ms icinde yerine oturur.
- Slot sayisi 48'de sabit; VST parametresinin secenek listesi calisma aninda
  degistirilemedigi icin bu sayi derleme zamaninda belirlenir.
- Fabrika pattern'leri filtre lane'ini kullanmiyor. Yeni fabrika pattern'i eklemek kullanici
  slotlarinin numarasini kaydirir (kayitli projelerde secili slot degisir), bu yuzden
  bilerek eklenmedi.
- Swing yalnizca 1/16'lik ciftlere uygulanir (1/8 swing yok).
- LP / HP gecisi aniden olur; calarken degistirmek kisa bir tik yapabilir.
- Rezonansli filtre hizli taranirken kesim frekansinda kisa bir "zap" duyulur - rezonansli
  filtrenin kendi sesi; SMOOTH'u uzatmak ya da RESO'yu azaltmak yumusatir.
