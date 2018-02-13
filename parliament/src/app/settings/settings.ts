export class Notifier {
  fields: [{ name: string }];
}

export class Settings {
  settings: {
    notifiers: [Notifier];
  };
}
