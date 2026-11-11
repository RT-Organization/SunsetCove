# Guida per l'Animatore — Prop, Strumenti e Braccia

**Progetto:** Sunset Cove  |  **Motore:** Unreal Engine 5.8  |  **Software:** Blender
**Per chi è:** l'animatore. Non serve sapere programmare e non serve conoscere Unreal.
**Documenti tecnici collegati (in inglese, per il programmatore):** `PropInventorySystem/Engine_Setup_Guide.md`, `ToolInventorySystem/Engine_Setup_Guide.md`.

> **In una frase:** tu costruisci tutto in Blender e lo esporti; il gioco riconosce ogni punto di attacco **dal suo nome**. Se i nomi sono esatti, in Unreal **non si deve sistemare niente a mano**. Il nome è il contratto.

---

## 1. Come funziona (in 1 minuto)

```
BLENDER                                   UNREAL (automatico)
─────────                                 ───────────────────
Braccia: ossa Prop_R, Prop_L,    ──FBX──► il gioco cerca queste ossa per NOME
         Tool_R, Tool_L                    e ci attacca gli oggetti
Oggetto: ossa Grip_R (e Grip_L)  ──FBX──► il gioco mette l'oggetto in modo che
                                           Grip_R coincida con Prop_R / Tool_R
Animazioni delle braccia         ──FBX──► il programmatore le collega agli eventi
Animazioni dell'oggetto          ──FBX──► l'oggetto le suona da solo
```

Cosa succede quando il giocatore prende un lettino con la mano destra:

1. Il gioco prende l'osso **`Grip_R`** del lettino e lo fa coincidere con l'osso **`Prop_R`** delle braccia (stessa posizione **e stessa rotazione**).
2. Da quel momento il lettino segue la mano: ogni animazione delle braccia sposta `Prop_R`, e il lettino la segue da solo. **Non devi animare il lettino insieme alle braccia.**
3. Il gioco fa partire l'animazione delle braccia giusta **per quell'oggetto e per quella mano** (es. `AS_Arms_Sunbed_Grab_R`).

**Ogni oggetto ha le sue animazioni.** Il lettino si prende in un modo, l'ombrellone in un altro, il secchio in un altro ancora: non esistono animazioni generiche valide per tutti. Il gioco sceglie l'animazione dal **nome dell'oggetto** (vedi sezione 7).

**Una mano occupata non blocca l'altra.** Se la mano destra tiene un lettino, la sinistra continua a fare i movimenti normali (idle, camminata, corsa...). Per questo le animazioni delle braccia si fanno **un braccio alla volta** (vedi sezione 4).

---

## 2. Regole generali (valgono per tutti i file)

- **Scala reale:** 1 metro in Blender = 100 cm in Unreal. Usa le **stesse impostazioni di scala** dei file che già esporti (es. `Export (blender)/Assets/Lettino.fbx`).
- **FPS:** gli stessi dei file già esportati (`AS_Idle`, `AS_Walk`...).
- **Niente root motion:** l'osso root non si muove mai nelle animazioni.
- **Pose di riferimento** (la posa a riposo del rig): vedi le regole per ogni tipo di file.
- **Nomi delle ossa:** niente spazi, niente accenti, niente caratteri speciali. Maiuscole/minuscole contano: `Grip_R` **non è** `grip_r`.
- **Non rinominare né cancellare mai** le ossa che esistono già nelle braccia: le animazioni già fatte si romperebbero. Si possono solo **aggiungere** ossa nuove.
- **Non creare mai socket a mano in Unreal.** Quelli dell'FBX sono gli unici validi. Se un socket va cambiato, si cambia in Blender e si esporta di nuovo.
- **Il pivot / origine della mesh non è importante** per props e strumenti: il gioco centra la mesh da solo.

---

## 3. Nomi e prefissi (convenzione Unreal)

Ogni file esportato deve avere il prefisso Unreal giusto. Il nome del file è il nome dell'asset in Unreal.

| Prefisso | Cosa è | Esempio | Chi lo crea |
|---|---|---|---|
| `SK_` | Skeletal Mesh (mesh con scheletro: **tutti** i nostri props e strumenti) | `SK_Sunbed`, `SK_Umbrella`, `SK_Bucket` | animatore |
| `SM_` | Static Mesh (senza scheletro) | non usato per props/strumenti | — |
| `AS_` | Animation Sequence (un'animazione) | `AS_Arms_Sunbed_Grab_R` | animatore |
| `AM_` | Animation Montage | `AM_Arms_Sunbed_Grab_R` | **programmatore** in Unreal, partendo dai tuoi `AS_` |
| `ABP_` | Animation Blueprint | `ABP_SunsetCoveCharacter` | programmatore |
| `PA_` | Physics Asset | `PA_Character` | Unreal (automatico) |
| `T_` / `M_` / `MI_` | Texture / Material / Material Instance | `T_Sunbed_BaseColor` | animatore / artista |

> **Eccezione:** la mesh delle braccia esiste già con il nome `SM_Character.fbx`. **Non rinominarla**: va reimportata sopra a quella esistente (sezione 8.1).

**Schema dei nomi delle animazioni** (il nome deve contenere l'oggetto, perché il gioco lo usa per scegliere):

```
Braccia:   AS_Arms_<Oggetto>_<Azione>_<Mano>      es. AS_Arms_Umbrella_Drop_L
Oggetto:   AS_<Oggetto>_<Azione>                  es. AS_Sunbed_Open
```

- `<Oggetto>` = `Sunbed`, `Umbrella`, `Bucket`, `Spray`, `Toy` (in inglese, una parola, maiuscola iniziale). Deve essere **identico** al nome del Blueprint e all'ID dello strumento: se cambia, avvisa il programmatore.
- `<Mano>` = `R` (destra) o `L` (sinistra).
- `<Azione>` = vedi la lista della sezione 9.

---

## 4. Le braccia in prima persona

### 4.1 Le ossa / socket da aggiungere

| Nome | Dove va | A cosa serve |
|---|---|---|
| `Prop_R` | nel palmo della mano **destra** | il punto dove la mano destra tiene un **prop** (lettino, ombrellone) |
| `Prop_L` | nel palmo della mano **sinistra** | idem, mano sinistra |
| `Tool_R` | nel palmo della mano **destra** | il punto dove la mano destra tiene uno **strumento** (secchio, spray, giocattolo). **Obbligatorio** |
| `Tool_L` | nel palmo della mano **sinistra** | riservato per il futuro: la mano sinistra prende cose che poi vanno dentro uno strumento (es. un granchio che poi si mette nel secchio, che è in `Tool_R`). Il codice non lo usa ancora, ma **va già creato** |

- **`Prop_R` e `Tool_R` possono stare in punti leggermente diversi** (tenere un secchio non è come tenere un lettino), ma tutti devono seguire la stessa regola degli assi (4.2).
- **Osso o socket?** Vanno bene entrambi, il gioco usa quello che trova con quel nome. **L'osso è la scelta più sicura**: crea un osso figlio dell'osso della mano e chiamalo `Prop_R`. Se invece usi un socket di Blender, è un **Empty** figlio dell'osso della mano con il prefisso `SOCKET_` (es. `SOCKET_Prop_R`): Unreal toglie il prefisso e il socket si chiama `Prop_R`.
- **Non devono essere animati da soli:** sono figli della mano e la seguono. Nessuna chiave su di loro.

### 4.2 Gli assi (regola uguale per TUTTI i punti di attacco)

Questa è la parte più importante: **`Grip_R` dell'oggetto deve avere gli stessi assi di `Prop_R` / `Tool_R`**, altrimenti l'oggetto in mano risulta ruotato.

Convenzione proposta, da usare **ovunque** (mani e oggetti):

- **X** = lungo le dita, verso la punta (in avanti)
- **Z** = fuori dal dorso della mano, verso l'alto
- **Y** = di lato (completa la terna)

Metodo pratico per non sbagliare (consigliato):

1. Metti le braccia nella loro posa di riferimento.
2. Metti l'oggetto **nella mano**, come lo vuoi vedere tenuto.
3. Crea `Grip_R` copiando posizione e rotazione di `Prop_R` (o `Tool_R`), ad esempio con *Copy Transforms*, poi applica.
4. Riporta l'oggetto dove deve stare e aggancia `Grip_R` all'osso root dell'oggetto.

Così, quando il gioco fa coincidere `Grip_R` con `Prop_R`, l'oggetto torna esattamente nella posa in cui l'hai messo tu.

### 4.3 Animare un braccio alla volta

Le animazioni delle braccia **muovono solo le ossa di UN braccio** (dalla clavicola/spalla fino alle dita, comprese le ossa delle dita), mai entrambe:

- `…_R` → chiavi solo sul braccio destro. L'altro braccio e il resto dello scheletro **non hanno chiavi** (lascia la posa di riferimento).
- `…_L` → chiavi solo sul braccio sinistro.

Perché: il gioco applica l'animazione solo a quel braccio e lascia l'altro libero di continuare idle/camminata/corsa. Se in un `…_R` muovi anche il braccio sinistro, il sinistro "scatta" quando l'animazione parte.

**Consegna al programmatore (una volta sola):** scrivi i nomi esatti delle **due ossa da cui parte ciascun braccio** (es. la clavicola destra e quella sinistra). Servono per dire a Unreal "applica questa animazione solo da qui in giù".

Anche se il sinistro è lo specchio del destro, **consegna sempre due file separati** (`_R` e `_L`).

### 4.4 Pose di riferimento delle braccia

Non cambiare la posa di riferimento delle braccia esistenti. Le ossa `Prop_R`, `Prop_L`, `Tool_R`, `Tool_L` si **aggiungono** alla struttura attuale.

---

## 5. I prop (lettino, ombrellone)

Ogni prop ha **il suo scheletro**, perché si anima quando viene piazzato (il lettino si apre, l'ombrellone si apre).

### 5.1 Le ossa da aggiungere

| Nome | Dove va | A cosa serve |
|---|---|---|
| `Grip_R` | dove la **mano destra** impugna il prop | il gioco la fa coincidere con `Prop_R` |
| `Grip_L` | dove la **mano sinistra** impugna il prop | il gioco la fa coincidere con `Prop_L`. Il lettino si tiene in un modo con la destra e in un altro con la sinistra, quindi **creala sempre**, anche se è la copia speculare della destra. (Se manca, il gioco usa `Grip_R` anche per la sinistra) |

Regole:

- Sono **ossa figlie dell'osso root** del prop e **non vengono mai animate** (nessuna chiave in nessuna animazione). Il gioco legge la loro posizione nel momento in cui il giocatore prende l'oggetto.
- Disegna la **posa chiusa/da trasporto** in modo che l'impugnatura sia dove si trova `Grip_R` / `Grip_L`: il prop deve risultare bene in mano proprio con quella posa.
- Gli assi seguono la regola 4.2.
- Tutte le altre ossa che servono all'animazione (le stecche dell'ombrellone, le gambe del lettino) sono libere.

### 5.2 La posa di riferimento del prop

La **posa di riferimento** (rest pose) del prop è **il prop APERTO / PIAZZATO**, come si vede nel livello. È la posa che il gioco mostra finché nessuna animazione è in corso.

### 5.3 Le animazioni del prop (le suona il prop da solo)

| File | Quando parte | Note |
|---|---|---|
| `AS_<Prop>_Close` | il giocatore prende il prop | da aperto a **chiuso**. Parte **a terra, dove si trova il prop**, e il prop si attacca alla mano **solo quando l'animazione è finita**. Finisce nella posa chiusa e **resta lì** mentre è in mano |
| `AS_<Prop>_Open` | il giocatore lo piazza a terra | da chiuso a **aperto**. Parte quando il prop appare nel punto scelto. Finisce nella posa di riferimento |

Esempi: `AS_Sunbed_Close`, `AS_Sunbed_Open`, `AS_Umbrella_Close`, `AS_Umbrella_Open`.

**La durata di `Close` decide quanto aspetta il gioco** prima di mettere il prop in mano (il prop si attacca quando sono finite sia `Close` sia l'animazione delle braccia `Grab`, la più lunga delle due). Quindi tienila **breve e leggibile** (circa 0,3–0,6 s) e falla durare **come `AS_Arms_<Prop>_Grab_R/L`**.

Un prop **lasciato cadere (Q) o lanciato resta chiuso** finché non viene piazzato: quando lo si riprende, `Close` non parte (è già chiuso).

**Il prop ora vola** (nessuna impostazione da fare per l'animatore): quando lo si prende, mentre parte `Close` il prop **vola** dal punto in cui si trova fino alla mano; quando lo si piazza, esce dalla mano e **vola** fino al punto scelto mentre parte `Open`. La durata del volo è **la più lunga tra un minimo (circa 0,4 s) e la durata di `Close` / `Open`**: per questo vanno tenute sui **0,3–0,5 s**. Il prop atterra esattamente dove era l'ologramma e solo allora diventa utilizzabile dai visitatori.

**Pose facoltative** (un solo fotogramma ciascuna): `AS_<Prop>_ClosedPose` (prop chiuso) e `AS_<Prop>_OpenPose` (prop aperto). Il gioco le usa solo per: l'ologramma di piazzamento (che deve mostrare il prop **aperto**), la posa di partenza del prop nel livello e per mettere il prop subito nella posa giusta se l'azione viene interrotta (per esempio premendo Q mentre si chiude). Se non ci sono, il gioco funziona lo stesso (la posa di riferimento è quella aperta).

In `Close`, `Open` e nelle pose l'osso `Grip_R` / `Grip_L` **non deve avere chiavi**: il gioco ne legge la posizione quando il prop viene preso.

---

## 6. Gli strumenti (secchio, spray, giocattolo)

Stesso principio dei prop: ogni strumento ha **il suo scheletro** e le sue animazioni.

- Gli strumenti si tengono **solo con la mano destra**: l'osso è **`Grip_R`** (niente `Grip_L`), e si aggancia a **`Tool_R`**.
- `Grip_R` segue le stesse regole del punto 5.1 (figlio del root, mai animato, assi come 4.2).
- La posa di riferimento dello strumento è **come lo si tiene in mano**, pronto all'uso.
- Le animazioni dello strumento sono **facoltative** (se l'oggetto ha parti che si muovono, es. la leva dello spray):

| File | Quando parte | Note |
|---|---|---|
| `AS_<Strumento>_Equip` | quando viene tirato fuori | facoltativa |
| `AS_<Strumento>_Use` | a ogni utilizzo | facoltativa (es. la leva dello spray che si preme) |

Esempi: `AS_Spray_Use`, `AS_Bucket_Equip`.

---

## 7. Come comunicano con Unreal

| Cosa fai in Blender | Cosa fa il gioco |
|---|---|
| Crei `Prop_R`, `Prop_L`, `Tool_R`, `Tool_L` sulle braccia | cerca queste ossa per nome e ci attacca gli oggetti |
| Crei `Grip_R` (e `Grip_L`) sull'oggetto | fa coincidere `Grip_R` con `Prop_R` o `Tool_R` (e `Grip_L` con `Prop_L`): stessa posizione e rotazione. Se gli assi sono uguali l'oggetto sta perfetto, senza ritocchi |
| Esporti `AS_Arms_<Oggetto>_<Azione>_<Mano>` | il programmatore li collega agli eventi del gioco: **per ogni oggetto e ogni mano** parte l'animazione col nome giusto. Il nome dell'oggetto nel file è la chiave che sceglie l'animazione |
| Il braccio "libero" non ha chiavi | continua a fare idle / camminata / corsa mentre l'altro braccio fa l'animazione dell'oggetto |
| Esporti `AS_<Prop>_Close` / `_Open` | il prop li suona da solo, sul proprio scheletro, quando viene preso / piazzato |
| Esporti `SK_<Oggetto>` con il nome giusto | l'oggetto ha già collisione, fisica, grip e ologramma: il programmatore crea un solo Blueprint e sceglie la mesh |

### Gli eventi del gioco (quando partono le animazioni delle braccia)

| Evento | Cosa succede nel gioco | Animazione delle braccia |
|---|---|---|
| **Grab** | il prop va in mano (E) | `…_Grab_R/L` |
| (in mano) | il prop è tenuto | `…_Hold_R/L` (loop) |
| **Place** | il prop viene posato a terra | `…_Place_R/L` |
| **Throw charge** | tasto di lancio tenuto: la barra si carica | `…_ThrowCharge_R/L` (posa tenuta, vedi 9.1) |
| **Throw** | tasto di lancio rilasciato | `…_Throw_R/L` |
| **Drop** | tasto Q: il prop cade dalla mano | `…_Drop_R/L` |
| **Equip** | uno strumento viene tirato fuori | `…_Equip_R` |
| **Holster** | lo strumento viene rimesso via | `…_Holster_R` |
| (in mano) | lo strumento è tenuto | `…_Hold_R` (loop) |
| **Use** | si usa lo strumento (tasto sinistro) | `…_Use_R` |

Tre cose da sapere (per costruire le animazioni):

1. **L'oggetto compare/lascia la mano subito**, nell'istante dell'evento: nel **Grab** il prop è già in mano all'inizio dell'animazione, nel **Place / Throw / Drop** lascia la mano all'inizio. Quindi consegna animazioni **brevi e leggibili** (circa 0,3–0,6 s). Se in seguito serve ritardare l'attacco o il distacco a un frame preciso, lo farà il programmatore: **dimmi in che frame** l'oggetto dovrebbe staccarsi/attaccarsi (annotalo nelle note di consegna).
2. L'animazione delle braccia **non sposta il giocatore**: niente root motion.
3. Le animazioni di un braccio vanno pensate per **funzionare insieme a quelle dell'altro braccio**: il giocatore può tenere un lettino a destra e un ombrellone a sinistra, ognuno con la sua animazione.

---

## 8. Esportare da Blender

> Le impostazioni qui sotto sono quelle che **contano per il nostro sistema**. Per tutto il resto (assi, scala, ecc.) **usa le stesse impostazioni dei file che già esporti** e che già funzionano in Unreal (`Export (blender)/Character/…`). Alla prima consegna di ogni tipo di file, fai **una prova**: esporta, importa in Unreal e controlla l'elenco ossa (sezione 11).

**Dove vanno i file** (cartella `Export (blender)` nel repository, quella usata di solito; i `.blend` sorgenti restano in `Blender/`):

```
Export (blender)/
├── Character/
│   ├── MeshCharacter/SM_Character.fbx                 ← le braccia (rimpiazza il file esistente)
│   ├── AnimazioniCharacter/                           ← idle/walk/run/jump (già esistente, non toccare)
│   └── AnimazioniInventario/
│       ├── Sunbed/    AS_Arms_Sunbed_Grab_R.fbx ...
│       ├── Umbrella/  AS_Arms_Umbrella_Grab_R.fbx ...
│       ├── Bucket/    AS_Arms_Bucket_Equip_R.fbx ...
│       ├── Spray/     ...
│       └── Toy/       ...
├── Props/
│   ├── Sunbed/    SK_Sunbed.fbx, AS_Sunbed_Open.fbx, AS_Sunbed_Close.fbx
│   └── Umbrella/  SK_Umbrella.fbx, AS_Umbrella_Open.fbx, AS_Umbrella_Close.fbx
└── Tools/
    ├── Bucket/    SK_Bucket.fbx, ...
    ├── Spray/     SK_Spray.fbx, AS_Spray_Use.fbx
    └── Toy/       SK_Toy.fbx
```

Il programmatore importa **da lì**: tu metti i file in quella cartella e basta. Non serve importare niente in Unreal.

### 8.1 Esportare le braccia (mesh + scheletro)

1. Controlla che ci siano le ossa `Prop_R`, `Prop_L`, `Tool_R`, `Tool_L` (sezione 4) e che gli assi siano giusti.
2. Esporta in FBX con il nome **`SM_Character.fbx`**, nella cartella `Character/MeshCharacter/` (sovrascrive quello esistente).
3. Impostazioni da controllare:
   - Oggetti da esportare: **Armature + Mesh** (+ **Empty** solo se hai usato i socket `SOCKET_`).
   - **Add Leaf Bones: DISATTIVATO** (altrimenti compaiono ossa in più).
   - **Only Deform Bones: DISATTIVATO** (se è attivo, `Prop_R` / `Tool_R` e simili **non vengono esportati** perché non deformano la mesh: è l'errore più comune).
   - Niente animazioni dentro il file della mesh (**Bake Animation** disattivato).
4. Il programmatore reimporta **sopra** la mesh esistente (non crea un duplicato), mantenendo lo scheletro.

### 8.2 Esportare un prop o uno strumento (mesh + scheletro)

1. Verifica `Grip_R` (e `Grip_L` per i prop), mai animate, figlie del root, con la regola degli assi.
2. Verifica la **posa di riferimento** (prop: aperto; strumento: come in mano).
3. File: `SK_<Oggetto>.fbx` in `Props/<Oggetto>/` oppure `Tools/<Oggetto>/`.
4. Impostazioni: stesse della sezione 8.1 (**Add Leaf Bones** e **Only Deform Bones** disattivati, **Armature + Mesh**, niente animazioni nel file).
5. Texture e materiali: con il prefisso della sezione 3.

### 8.3 Esportare le animazioni

1. **Un file per ogni animazione**, con il nome della sezione 9 (es. `AS_Arms_Sunbed_Grab_R.fbx`).
2. Ogni file contiene **una sola azione** (Action) di Blender, sullo scheletro corretto:
   - animazioni delle braccia → scheletro delle braccia;
   - `AS_<Prop>_Open/Close`, `AS_<Strumento>_…` → scheletro **di quell'oggetto**.
3. Impostazioni:
   - **Bake Animation: ATTIVO**, **All Actions: DISATTIVATO** (altrimenti finiscono nel file anche le altre azioni), **NLA Strips: DISATTIVATO**.
   - **Simplify: 0** (non semplificare le curve).
   - Frame iniziale e finale = **lunghezza reale** dell'animazione (niente frame in più).
   - Stesso scheletro, **stesse ossa e stessi nomi** del file della mesh.
4. Controlla che l'animazione sia **solo del braccio giusto** (sezione 4.3) e che `Grip_*` / `Prop_*` / `Tool_*` non abbiano chiavi.

### 8.4 Se devi riesportare

Riesporta **lo stesso nome di file**: il programmatore reimporta sopra l'asset esistente. Non rinominare mai un file già consegnato senza avvisare (i collegamenti nel gioco usano il nome).

---

## 9. Lista delle animazioni

Legenda: **R/L** = si consegna un file per ogni mano. **Loop** = si ripete; **Una volta** = parte e finisce; **Tenuta** = finisce in una posa che resta fissa finché serve. **Priorità: A** = serve per far funzionare il sistema; **B** = bello da avere / dopo.

### 9.1 Braccia — prop (lettino `Sunbed` e ombrellone `Umbrella`)

Per **ciascun prop** e **ciascuna mano (R e L)**:

| Azione | Nome (es. lettino, destra) | Tipo | Quando | Pr. |
|---|---|---|---|---|
| Prendere | `AS_Arms_Sunbed_Grab_R` | Una volta | E sul prop | A |
| In mano | `AS_Arms_Sunbed_Hold_R` | **Loop** | prop in quella mano | A |
| Piazzare | `AS_Arms_Sunbed_Place_R` | Una volta | il prop viene posato (tasto sinistro rilasciato su ologramma verde) | A |
| Carica del lancio | `AS_Arms_Sunbed_ThrowCharge_R` | **Tenuta** | tasto destro tenuto, la barra di forza sale e scende | A |
| Lancio | `AS_Arms_Sunbed_Throw_R` | Una volta | tasto destro rilasciato | A |
| Lasciare | `AS_Arms_Sunbed_Drop_R` | Una volta | Q | A |
| Tenere con ologramma | `AS_Arms_Sunbed_PlaceAim_R` | Loop | tasto di piazzamento tenuto (l'ologramma è visibile) | B |

Totale per prop: 6 animazioni × 2 mani = **12 file**, più la B. Con 2 prop: **24 file** (+ 4 opzionali).

**La carica del lancio (`ThrowCharge`) non è un loop.** È una **posa tenuta**: parte dalla posa `Hold` e porta il braccio **indietro, come per lanciare**, e **finisce lì**, fermo, nella posa di carica. Il gioco la tiene sull'ultimo frame finché il giocatore non rilascia (la barra di forza sale e scende, ma il braccio resta fermo indietro). Quando il giocatore rilascia parte **`Throw`**, che **inizia dalla stessa posa di carica** (identica, per non avere salti) e fa il gesto del lancio, poi torna verso `Hold` / idle.

### 9.2 Braccia — strumenti (`Bucket`, `Spray`, `Toy`)

Solo **mano destra** (`_R`; gli strumenti si tengono con `Tool_R`). Per **ciascuno strumento**:

| Azione | Nome (es. secchio) | Tipo | Quando | Pr. |
|---|---|---|---|---|
| Tirare fuori | `AS_Arms_Bucket_Equip_R` | Una volta | lo strumento viene scelto dal menu (rilascio di Tab) | A |
| Rimettere via | `AS_Arms_Bucket_Holster_R` | Una volta | lo strumento viene scelto di nuovo / Tab senza selezione | A |
| In mano | `AS_Arms_Bucket_Hold_R` | **Loop** | strumento equipaggiato | A |
| Usare | `AS_Arms_Bucket_Use_R` | Una volta (lo spray: **Loop**) | tasto sinistro | A |
| Raccogliere da terra | `AS_Arms_Bucket_Pickup_R` | Una volta | E su uno strumento a terra | B |
| Lasciare | `AS_Arms_Bucket_Drop_R` | Una volta | Q con il menu aperto sullo strumento | B |

Note per ogni strumento: **secchio** = il gesto di raccogliere un granchio; **spray** = spruzzare (tenuto: si ripete finché si preme); **giocattolo** = porgerlo. Se uno strumento si usa in modo diverso, descrivi il gesto al programmatore.

Totale: 4 animazioni obbligatorie × 3 strumenti = **12 file** (+ 6 opzionali).

### 9.3 Braccia — menu radiale (generiche, non per oggetto)

Solo se le vuoi: il menu radiale (Tab tenuto) è lo stesso per tutti.

| Nome | Tipo | Quando | Pr. |
|---|---|---|---|
| `AS_Arms_ToolMenu_Open_R` | Una volta | Tab premuto (il braccio si abbassa / si rilassa) | B |
| `AS_Arms_ToolMenu_Close_R` | Una volta | Tab rilasciato | B |

### 9.4 Dell'oggetto (le suona l'oggetto da solo)

| Nome | Tipo | Quando | Pr. |
|---|---|---|---|
| `AS_Sunbed_Close`, `AS_Umbrella_Close` | Una volta (finisce **chiuso**) | il prop viene preso (parte a terra, poi il prop va in mano) | A |
| `AS_Sunbed_Open`, `AS_Umbrella_Open` | Una volta (finisce **aperto**) | il prop viene piazzato | A |
| `AS_Bucket_Equip`, `AS_Spray_Equip`, `AS_Toy_Equip` | Una volta | lo strumento viene tirato fuori | B |
| `AS_Bucket_Use`, `AS_Spray_Use`, `AS_Toy_Use` | Una volta | a ogni uso | B |

Le animazioni dell'oggetto e quelle delle braccia che partono nello stesso istante (es. `AS_Arms_Sunbed_Place_R` e `AS_Sunbed_Open`) è meglio che abbiano **la stessa durata**.

### 9.5 Riepilogo del lavoro

| Gruppo | File obbligatori (A) |
|---|---|
| Braccia – prop (2 prop × 2 mani × 6) | 24 |
| Braccia – strumenti (3 strumenti × 4) | 12 |
| Dell'oggetto – prop (2 × 2) | 4 |
| **Totale A** | **40** |

I file **B** sono opzionali e si possono aggiungere dopo senza cambiare niente nel gioco.

### 9.6 Non nella lista (per ora)

- Granchio preso con la mano sinistra e messo nel secchio (usa `Tool_L`): sarà una lista a parte quando decidiamo il gesto.
- Sacco dei rifiuti: tolto per ora.
- Animazioni del vento sui prop: non servono.

### 9.7 Checklist completa di tutte le animazioni (da spuntare)

Spunta ogni riga quando il file è **fatto ed esportato** nella cartella giusta. **A** = obbligatoria, **B** = facoltativa. Ogni animazione delle braccia ha una riga per la mano destra (`_R`) e una per la sinistra (`_L`), perché sono file separati.

#### Braccia — Lettino (`Sunbed`) — 12 obbligatorie + 2 facoltative

- [ ] `AS_Arms_Sunbed_Grab_R` — **A**
- [ ] `AS_Arms_Sunbed_Grab_L` — **A**
- [ ] `AS_Arms_Sunbed_Hold_R` — **A**
- [ ] `AS_Arms_Sunbed_Hold_L` — **A**
- [ ] `AS_Arms_Sunbed_Place_R` — **A**
- [ ] `AS_Arms_Sunbed_Place_L` — **A**
- [ ] `AS_Arms_Sunbed_ThrowCharge_R` — **A**
- [ ] `AS_Arms_Sunbed_ThrowCharge_L` — **A**
- [ ] `AS_Arms_Sunbed_Throw_R` — **A**
- [ ] `AS_Arms_Sunbed_Throw_L` — **A**
- [ ] `AS_Arms_Sunbed_Drop_R` — **A**
- [ ] `AS_Arms_Sunbed_Drop_L` — **A**
- [ ] `AS_Arms_Sunbed_PlaceAim_R` — **B**
- [ ] `AS_Arms_Sunbed_PlaceAim_L` — **B**

#### Braccia — Ombrellone (`Umbrella`) — 12 obbligatorie + 2 facoltative

- [ ] `AS_Arms_Umbrella_Grab_R` — **A**
- [ ] `AS_Arms_Umbrella_Grab_L` — **A**
- [ ] `AS_Arms_Umbrella_Hold_R` — **A**
- [ ] `AS_Arms_Umbrella_Hold_L` — **A**
- [ ] `AS_Arms_Umbrella_Place_R` — **A**
- [ ] `AS_Arms_Umbrella_Place_L` — **A**
- [ ] `AS_Arms_Umbrella_ThrowCharge_R` — **A**
- [ ] `AS_Arms_Umbrella_ThrowCharge_L` — **A**
- [ ] `AS_Arms_Umbrella_Throw_R` — **A**
- [ ] `AS_Arms_Umbrella_Throw_L` — **A**
- [ ] `AS_Arms_Umbrella_Drop_R` — **A**
- [ ] `AS_Arms_Umbrella_Drop_L` — **A**
- [ ] `AS_Arms_Umbrella_PlaceAim_R` — **B**
- [ ] `AS_Arms_Umbrella_PlaceAim_L` — **B**

#### Braccia — Secchio (`Bucket`), solo mano destra — 4 obbligatorie + 2 facoltative

- [ ] `AS_Arms_Bucket_Equip_R` — **A**
- [ ] `AS_Arms_Bucket_Holster_R` — **A**
- [ ] `AS_Arms_Bucket_Hold_R` — **A**
- [ ] `AS_Arms_Bucket_Use_R` — **A**
- [ ] `AS_Arms_Bucket_Pickup_R` — **B**
- [ ] `AS_Arms_Bucket_Drop_R` — **B**

#### Braccia — Spray (`Spray`), solo mano destra — 4 obbligatorie + 2 facoltative

- [ ] `AS_Arms_Spray_Equip_R` — **A**
- [ ] `AS_Arms_Spray_Holster_R` — **A**
- [ ] `AS_Arms_Spray_Hold_R` — **A**
- [ ] `AS_Arms_Spray_Use_R` (Loop) — **A**
- [ ] `AS_Arms_Spray_Pickup_R` — **B**
- [ ] `AS_Arms_Spray_Drop_R` — **B**

#### Braccia — Giocattolo (`Toy`), solo mano destra — 4 obbligatorie + 2 facoltative

- [ ] `AS_Arms_Toy_Equip_R` — **A**
- [ ] `AS_Arms_Toy_Holster_R` — **A**
- [ ] `AS_Arms_Toy_Hold_R` — **A**
- [ ] `AS_Arms_Toy_Use_R` — **A**
- [ ] `AS_Arms_Toy_Pickup_R` — **B**
- [ ] `AS_Arms_Toy_Drop_R` — **B**

#### Braccia — Menu radiale (generiche) — facoltative

- [ ] `AS_Arms_ToolMenu_Open_R` — **B**
- [ ] `AS_Arms_ToolMenu_Close_R` — **B**

#### Dell'oggetto (sul suo scheletro)

- [ ] `AS_Sunbed_Close` — **A**
- [ ] `AS_Sunbed_Open` — **A**
- [ ] `AS_Umbrella_Close` — **A**
- [ ] `AS_Umbrella_Open` — **A**
- [ ] `AS_Bucket_Equip` — **B**
- [ ] `AS_Bucket_Use` — **B**
- [ ] `AS_Spray_Equip` — **B**
- [ ] `AS_Spray_Use` — **B**
- [ ] `AS_Toy_Equip` — **B**
- [ ] `AS_Toy_Use` — **B**

**Totale obbligatorie (A): 40** (24 braccia prop + 12 braccia strumenti + 4 oggetto). **Facoltative (B): 18** (4 `PlaceAim`, 6 `Pickup`/`Drop` degli strumenti, 2 menu radiale, 6 dell'oggetto).

---

## 10. Checklist di consegna (spunta prima di mandare un file)

**Per le braccia:**
- [ ] ci sono `Prop_R`, `Prop_L`, `Tool_R`, `Tool_L`, nel palmo, con gli assi della regola 4.2
- [ ] nessun osso esistente rinominato o cancellato
- [ ] **Add Leaf Bones** e **Only Deform Bones** disattivati
- [ ] file `SM_Character.fbx` nella cartella `Character/MeshCharacter/`

**Per ogni oggetto (prop o strumento):**
- [ ] ha il suo scheletro, nome `SK_<Oggetto>.fbx`
- [ ] `Grip_R` (e `Grip_L` per i prop): figli del root, **mai animati**, assi uguali a `Prop_R` / `Tool_R`
- [ ] posa di riferimento: prop **aperto**, strumento **come in mano**
- [ ] nomi senza spazi, con il prefisso giusto

**Per ogni animazione:**
- [ ] nome giusto, con l'oggetto dentro: `AS_Arms_<Oggetto>_<Azione>_<Mano>`
- [ ] **solo un braccio** con chiavi (nelle animazioni delle braccia)
- [ ] niente chiavi su `Grip_*`, `Prop_*`, `Tool_*`, niente root motion
- [ ] un solo file per animazione, una sola azione dentro, **Bake ON / All Actions OFF**
- [ ] lunghezza giusta (nessun frame in più)
- [ ] `ThrowCharge` finisce nella posa di carica; `Throw` parte dalla stessa posa
- [ ] annotato in che frame l'oggetto dovrebbe staccarsi / attaccarsi (se serve)
- [ ] messo nella cartella giusta di `Export (blender)`

---

## 11. Come controllare che sia andato tutto bene (dopo l'importazione)

Dopo che il programmatore ha importato, in Unreal: apri la mesh → **Skeleton Tree**. Devi vedere:

- sulle braccia: `Prop_R`, `Prop_L`, `Tool_R`, `Tool_L`;
- su un prop: `Grip_R` e `Grip_L`; su uno strumento: `Grip_R`.

Se manca uno di questi, il gioco lo scrive nel log all'avvio e usa un punto di ripiego (l'oggetto "galleggia" davanti alle braccia o è tenuto dal centro).

---

## 12. Problemi comuni

| Sintomo | Causa probabile | Cosa fare |
|---|---|---|
| In Unreal non trovo `Prop_R` / `Tool_R` | **Only Deform Bones** era attivo, oppure il nome è diverso (maiuscole, spazi) | disattiva l'opzione, controlla il nome, riesporta |
| L'oggetto in mano è ruotato | `Grip_R` e `Prop_R` / `Tool_R` hanno **assi diversi** | rifai `Grip_R` copiando la trasformazione dell'osso della mano (4.2) |
| L'oggetto è in mano ma più in alto / basso del previsto | `Grip_R` non è dove la mano tiene l'oggetto | sposta `Grip_R` in Blender |
| L'oggetto cambia posizione nella mano dopo che si apre/chiude | `Grip_R` ha chiavi nelle animazioni | togli le chiavi: deve restare fermo |
| Il braccio libero "scatta" quando parte un'animazione | l'animazione ha chiavi anche sull'altro braccio | elimina le chiavi dell'altro braccio |
| Il braccio non si muove in Unreal | animazione importata sullo scheletro sbagliato (non quello delle braccia) | controlla di aver animato lo scheletro giusto |
| Le braccia si deformano / ossa in più | **Add Leaf Bones** era attivo | disattiva e riesporta |
| Il lancio parte con un salto della posa | `Throw` non inizia dalla stessa posa dove finisce `ThrowCharge` | fai coincidere il primo frame di `Throw` con l'ultimo di `ThrowCharge` |
| L'oggetto appare troppo grande / piccolo | scala diversa dai file già funzionanti | usa le stesse impostazioni di scala di `Lettino.fbx` |

---

## 13. Cosa NON fare

- ❌ Creare o spostare socket a mano **in Unreal**.
- ❌ Rinominare o cancellare ossa già consegnate.
- ❌ Mettere chiavi su `Grip_R`, `Grip_L`, `Prop_*`, `Tool_*`.
- ❌ Usare il root motion.
- ❌ Fare animazioni "generiche": ogni oggetto ha le sue.
- ❌ Chiavi su entrambe le braccia nello stesso file.
- ❌ Cambiare il nome di un file già consegnato senza avvisare.

---

## Appendice — Riferimento rapido dei nomi

| Cosa | Nome |
|---|---|
| Punto mano destra per prop | `Prop_R` |
| Punto mano sinistra per prop | `Prop_L` |
| Punto mano destra per strumenti | `Tool_R` |
| Punto mano sinistra per strumenti (futuro) | `Tool_L` |
| Impugnatura sull'oggetto, mano destra | `Grip_R` |
| Impugnatura sull'oggetto, mano sinistra (prop) | `Grip_L` |
| Prefisso del socket di Blender (se non è un osso) | `SOCKET_` |
| Oggetti | `Sunbed`, `Umbrella`, `Bucket`, `Spray`, `Toy` |
| Azioni prop (braccia) | `Grab`, `Hold`, `Place`, `ThrowCharge`, `Throw`, `Drop` (`PlaceAim` facoltativa) |
| Azioni strumenti (braccia) | `Equip`, `Holster`, `Hold`, `Use` (`Pickup`, `Drop` facoltative) |
| Azioni dell'oggetto | `Open`, `Close` (prop); `Equip`, `Use` (strumenti, facoltative) |
