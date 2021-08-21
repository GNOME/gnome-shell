import { setConsoleLogDomain } from 'console';

setConsoleLogDomain('GNOME Shell');

imports.ui.environment.init();
imports.ui.main.start();
