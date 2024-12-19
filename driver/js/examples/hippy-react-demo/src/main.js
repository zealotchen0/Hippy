import { Hippy, NetworkModule } from '@hippy/react';
import App from './app';

global.Hippy.on('uncaughtException', (err) => {
  console.error('uncaughtException error', err.stack, err.message);
});

// only supported in iOS temporarily
global.Hippy.on('unhandledRejection', (reason) => {
  console.error('unhandledRejection reason', reason);
});

new Hippy({
  appName: 'Demo',
  entryPage: App,
  // set global bubbles, default is false
  bubbles: false,
  // set log output, default is false
  silent: false,
}).start();

NetworkModule.getCookies('https://hippyjs.org').then((cookies) => {
  console.log(`${cookies}123444`);
})
  .catch((error) => {
    console.log(`${error}123444error`);
  })
  .finally(() => {
    console.log('123444finally');
  });
