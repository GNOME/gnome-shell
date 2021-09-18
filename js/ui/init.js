import { setConsoleLogDomain } from 'console';

setConsoleLogDomain('GNOME Shell');

import "./environment.js";

import("./main.js")
  .then(({ main }) => {
    main.start();
  })
  .catch((error) => {
    logError(error);
  })
  .finally(() => {
    log("Main imported.");
  });