# Ashenveil

Ένα 3D fantasy RPG για κινητά (Android και iOS), στο ύφος των MMORPG όπως το World of Warcraft,
αλλά με **δικό του κόσμο, ιστορία, κλάσεις και ικανότητες**. Όλα τα ονόματα, τα μοντέλα, τα εικονίδια
και οι ήχοι είναι πρωτότυπα. Δεν υπάρχει υλικό τρίτων (εκτός από τη γραμματοσειρά Cinzel, άδεια OFL).

## Το παιχνίδι

- **3 κλάσεις**, η καθεμία με 4 δικές της ικανότητες που ξεκλειδώνουν στα επίπεδα 1, 2, 4 και 6:
  - **Stormblade**: μάχη σώμα με σώμα, αντέχει πολύ (Thunder Cleave, Skyward Leap, Static Guard, Tempest)
  - **Emberseer**: μάγος φωτιάς από απόσταση, μεγάλη ζημιά (Cinder Lance, Ash Ring, Phoenix Veil, Falling Star)
  - **Thornkeeper**: φύλακας της φύσης, θεραπεία και κάλεσμα λύκου (Bramble Snare, Renewal, Spirit Wolf, Wild Bloom)
- **Ανοιχτός κόσμος** με 6 περιοχές: το χωριό Hearthmoor, Duskwood Glade, Webhollow, το στρατόπεδο των Hollowkin,
  το Ashen Scar και η φωλιά του Varkul.
- **7 αποστολές** σε σειρά, με διαλόγους, ανταμοιβές και σημάδια `!` / `?` πάνω από τους NPC.
- **Επίπεδα 1 έως 10**, εμπειρία, χρυσός, λάφυρα με σπανιότητα (Common έως Epic), εξοπλισμός, πωλητής.
- **Εχθροί με τεχνητή νοημοσύνη**: περιπλανώνται, σε εντοπίζουν, φωνάζουν τους συντρόφους τους και γυρίζουν πίσω
  αν απομακρυνθούν πολύ. Ένα elite αφεντικό (Chieftain Gorran) και το τελικό boss **Varkul the Cindermaw**, με
  κόκκινους κύκλους που πρέπει να αποφύγεις, κάλεσμα βοηθών και έξαλλη φάση.
- **Χειρισμός αφής**: joystick με τον αριστερό αντίχειρα, περιστροφή κάμερας με τον δεξί, pinch για zoom,
  πάτημα σε εχθρό για στόχευση. Υποστηρίζεται και πληκτρολόγιο (WASD, 1-4, R, Tab, F).
- **Αυτόματη αποθήκευση**: κάθε 15 δευτερόλεπτα και όταν η εφαρμογή πάει στο παρασκήνιο.
- **Ρύθμιση γραφικών**: High ή Battery saver (για παλαιότερα κινητά).

## Τι έχει ελεγχθεί και τι όχι

| Τι | Κατάσταση |
|---|---|
| Κανόνες παιχνιδιού, αποστολές, αποθήκευση | 13 αυτόματα unit tests περνούν (`npm test`) |
| Όλο το παιχνίδι σε browser, σε μέγεθος κινητού | Το smoke test περνάει (`npm run smoke`): μενού, κίνηση με joystick, μάχη, αποστολές, boss, θάνατος, αποθήκευση |
| Ισορροπία δυσκολίας | Ένα bot τερματίζει όλη την ιστορία με **κάθε** κλάση (`npm run playthrough`) |
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
npm run playthrough   # bot που παίζει όλο το παιχνίδι με κάθε κλάση
npm run cap:sync      # build + αντιγραφή στο android/ και ios/
node tools/gen-assets.mjs   # ξαναφτιάχνει εικονίδια και splash screens
```

Δομή:
```
src/data/      κλάσεις, ικανότητες, εχθροί, περιοχές, NPC, αποστολές (εδώ αλλάζεις την ισορροπία)
src/game/      rules (στατιστικά/ζημιά/λάφυρα), progress (αποστολές/αποθήκευση), terrain, scene,
               models, units, game (μάχη, AI, ικανότητες, κάμερα), audio
src/ui/        HUD, οθόνες, χειρισμός αφής, εικονίδια
tests/         unit tests, smoke test, playthrough bot
android/ ios/  native projects (Capacitor)
```

Τεχνολογίες: TypeScript, Three.js (3D), Vite, Capacitor 8.
