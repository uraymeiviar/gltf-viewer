# NPM bundler

In this module we provide a way to integrate (some) NPM modules into the QML frontend.

Use npm (node package manager) to install modules and exectue the rollup script to make them available to the QML side.

This bundler is based on https://www.librehat.com/use-npm-packages-in-qml/

## Adding npm modules

First install the new module via NPM

```sh
npm install <modulename>
```

If you want to directly make the module available to the QML side, add the following lines to `src/thirdparty.ts`

```
import * as <modulename> from '<modulename>'
export { <modulename> };
```

Afterwards run `npm run build` in the `js` directory.


⚠️ The npm package might be incompatible with QML. If this is the case there will be errors when importing `thirdparty.qml.mjs` in a QML script or when using the imported properties. This is most likely a deal breaker for the npm package, search for an alternative.