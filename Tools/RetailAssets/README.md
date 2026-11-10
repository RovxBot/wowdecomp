# External Forever assets

Asset preparation is tracked in [#221](https://github.com/RovxBot/wowdecomp/issues/221).
This optional Linux x86-64 tooling reads an installed Forever client offline and
stages the bounded Alpha 0.2.0 UI/font inputs outside the product checkout. It is
not a client dependency or a CASC runtime mount. Keep the installation read-only.
Game assets never enter Git or ordinary Actions artifacts.

The `Offline retail asset tooling` workflow compiles the pinned MIT CascLib
reader without game data. Download its artifact, check the Actions ZIP digest,
and retain `manifest.json`, `LICENSE.CascLib` and `libcasc.so.1.0.0` together.
The source archive and commit are locked in `reader.json`; client/dependency
compilation stays in Actions. CascLib is used only offline, with downloads off
and strict read checks on. Missing encryption keys are errors.

Fonts, UI exports and later textures are runtime/release inputs, not C++ compiler
inputs. Their availability does not establish native format support, rendering,
secure control construction or gameplay. Those requirements remain #215/#220;
model/character assets remain #214.
