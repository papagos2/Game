# Ashenveil

Ένα 3D fantasy RPG για κινητά (Android και iOS), στο ύφος των MMORPG όπως το World of Warcraft,
αλλά με **δικό του κόσμο, ιστορία, κλάσεις και ικανότητες**. Όλα τα ονόματα, τα μοντέλα, τα εικονίδια
και οι ήχοι είναι πρωτότυπα. Δεν υπάρχει υλικό τρίτων (εκτός από τη γραμματοσειρά Cinzel, άδεια OFL).

## Το παιχνίδι

- **3 κλάσεις, 9 δρόμοι (specializations).** Κάθε κλάση έχει 4 βασικές ικανότητες. Στο **level 10** διαλέγεις
  έναν από 3 δρόμους, που φέρνει 2 νέες ικανότητες (στα levels 10 και 16) και δικό του δέντρο δεξιοτήτων:
  - **Stormblade**: *Tempest Knight* (κεραυνοί), *Bulwark* (ακλόνητος αμυντικός), *Spellblade* (υβριδικός: ατσάλι και φωτιά)
  - **Emberseer**: *Pyromancer* (φωτιά), *Frostweaver* (πάγος), *Thermancer* (υβριδικός: φωτιά και πάγος)
  - **Thornkeeper**: *Grovewarden* (θεραπεία), *Beastcaller* (αρκούδα και λύκος σύντροφοι), *Rotbloom* (δηλητήριο)
- **Skill tree (talents)**: 1 πόντος ανά level (29 συνολικά). Βασικό δέντρο κλάσης και δέντρο του δρόμου σου,
  με 3 βαθμίδες. Οι πόντοι ανεβάζουν ζημιά, cooldowns, ζωή, πανοπλία, critical, θεραπεία, συντρόφους κ.ά.
  Μπορείς να τους ξαναμοιράσεις (ή να αλλάξεις δρόμο) στον πωλητή, πληρώνοντας χρυσό.
- **3 χάρτες (πράξεις), levels 1–30**, ο καθένας με δικό του χωριό, εχθρούς, elite αρχηγό και boss:
  - Πράξη I: *Vale of Ashenveil* (1–10), boss **Varkul the Cindermaw**
  - Πράξη II: *Frostmarch* (10–20), χιονισμένος χάρτης, boss **Ysolde, the Rime Queen**
  - Πράξη III: *Sunscar Dunes* (20–30), έρημος, boss **Azhkar the Sunflayer**
  - Ταξιδεύεις μέσω του Wayfinder δίπλα στη φωτεινή πέτρα (waystone) κάθε χωριού.
- **23 αποστολές** (σκότωσε, μάζεψε, εξερεύνησε) με διαλόγους, ανταμοιβές και σημάδια `!` / `?`.
- **Εξοπλισμός σε 8 θέσεις**: όπλο, κράνος, θώρακας, γάντια, μπότες, περιδέραιο και 2 δαχτυλίδια.
  5 σπανιότητες (Common έως **Legendary**) και 9 θρυλικά αντικείμενα με όνομα από τα boss. Υλικά ανά κλάση
  (πανοπλία από ατσάλι, ύφασμα ή δέρμα), πολύτιμοι λίθοι στα κοσμήματα, στατιστικά Power, Stamina, Armor και Crit.
  Κουμπί «Equip best» και πωλητής με εξοπλισμό για το level σου.
- **Εχθροί με τεχνητή νοημοσύνη**: 21 είδη, με δηλητήριο, πάγωμα, κάψιμο, χτυπήματα στο έδαφος που πρέπει να
  αποφύγεις, και boss με κύκλους, βοηθούς και έξαλλη φάση.
- **Χειρισμός αφής**: joystick με τον αριστερό αντίχειρα, κάμερα με τον δεξί, pinch για zoom, πάτημα σε εχθρό
  για στόχευση. Πληκτρολόγιο: WASD, 1–6, R, Tab, F.
- **Αυτόματη αποθήκευση**, με μεταφορά των παλιών αποθηκεύσεων στη νέα μορφή.

## Τι έχει ελεγχθεί και τι όχι

| Τι | Κατάσταση |
|---|---|
| Κανόνες, talents, specializations, αντικείμενα, αποστολές, αποθήκευση | 23 αυτόματα unit tests περνούν (`npm test`) |
| Όλο το παιχνίδι σε browser, σε μέγεθος κινητού | Το smoke test περνάει (`npm run smoke`): μενού, κίνηση, μάχη, αποστολές, boss, θάνατος, επιλογή δρόμου, talents, ταξίδι στους 3 χάρτες, αποθήκευση |
| Ισορροπία δυσκολίας | Ένα bot τερματίζει **και τους 3 χάρτες (level 1–30) με όλους τους 9 δρόμους** (`npm run playthrough`), με 1–4 θανάτους ο καθένας. Οι Πράξεις II–III βγαίνουν μάλλον εύκολες για το bot, που αποφεύγει τέλεια· χρειάζεται δοκιμή από ανθρώπους |
| Android project | Δημιουργημένο (Capacitor 8, target SDK 36). Το APK χτίζεται αυτόματα στο GitHub Actions |
| iOS project | Δημιουργημένο (Xcode project + Swift Package Manager). **Χρειάζεται Mac με Xcode για build** |
| Δοκιμή σε πραγματικό κινητό | **Δεν έχει γίνει ακόμα.** Είναι το επόμενο βήμα |

## Πώς το βάζεις στο κινητό σου

### Android (ο πιο γρήγορος δρόμος)
1. Σε κάθε push, το GitHub χτίζει αυτόματα ένα APK: **Actions → Build Android APK → τελευταίο run → Artifacts →
   `ashenveil-debug-apk`**.
2. Κατέβασέ το στο κινητό, άνοιξε το zip και πάτα το `app-debug.apk`. Το Android θα ζητήσει να επιτρέψεις την
   εγκατάσταση από «άγνωστες πηγές» για αυτή τη φορά.

### Δοκιμή στον browser (χωρίς εγκατάσταση)
```bash
npm install
npm run dev
```
Άνοιξε τη διεύθυνση που εμφανίζεται (π.χ. `http://192.168.x.x:5173`) στο κινητό σου, στο ίδιο Wi-Fi.

### iOS
Χρειάζεσαι Mac με Xcode και (για εγκατάσταση σε iPhone) λογαριασμό Apple Developer.
```bash
npm install
npm run ios          # χτίζει το παιχνίδι και ανοίγει το Xcode
```
Στο Xcode: διάλεξε το Team σου στο *Signing & Capabilities* και πάτα Run με συνδεδεμένο iPhone.

### Android Studio (για release build)
```bash
npm run android      # χτίζει το παιχνίδι και ανοίγει το Android Studio
```

## Δημοσίευση στα καταστήματα (όταν είσαι έτοιμος)
- **Google Play**: λογαριασμός developer (εφάπαξ κόστος), υπογεγραμμένο release build (`.aab`) από το Android Studio,
  πολιτική απορρήτου, screenshots και περιγραφή.
- **App Store**: Apple Developer Program (ετήσια συνδρομή), build από Xcode, TestFlight για δοκιμή, έλεγχος από την Apple.
- Το `appId` είναι `com.papagos.ashenveil` (στο `capacitor.config.json`). Άλλαξέ το **πριν** την πρώτη δημοσίευση,
  αν θέλεις άλλο, γιατί μετά δεν αλλάζει.

## Για developers

```bash
npm install
npm run dev           # dev server με hot reload
npm test              # unit tests (vitest)
npm run build         # typecheck + production build στο dist/
npm run smoke         # end-to-end test σε headless Chromium (screenshots στο tests/output/)
npm run playthrough   # bot που παίζει όλο το παιχνίδι με κάθε δρόμο (ή: npm run playthrough frostweaver)
npm run cap:sync      # build + αντιγραφή στο android/ και ios/
node tools/gen-assets.mjs   # ξαναφτιάχνει εικονίδια και splash screens
```

Δομή:
```
src/data/      κλάσεις, δρόμοι, ικανότητες (ως δεδομένα), talents, αντικείμενα, εχθροί, χάρτες, αποστολές
src/game/      rules (στατιστικά/ζημιά/λάφυρα), progress (αποστολές/talents/αποθήκευση), abilities (μηχανή
               ικανοτήτων), terrain, scene, models, units, game (μάχη, AI, σύντροφοι, κάμερα), audio
src/ui/        HUD, οθόνες, χειρισμός αφής, εικονίδια
tests/         unit tests, smoke test, playthrough bot
android/ ios/  native projects (Capacitor)
```

Τεχνολογίες: TypeScript, Three.js (3D), Vite, Capacitor 8.

## Γραφικά (συνεργασία με άλλο εργαλείο)
Ο οδηγός [`docs/VISUALS.md`](docs/VISUALS.md) εξηγεί πού βρίσκεται κάθε οπτικό στοιχείο, τι επιτρέπεται να
αλλάξει χωρίς να χαλάσει το gameplay και ποια τεστ πρέπει να περνάνε. Δώσε τον στο ChatGPT ή σε όποιον
δουλέψει τα γραφικά.
