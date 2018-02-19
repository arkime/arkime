export class Alert {
  id: string;
  on: boolean;
  name: string;
  description: string;
}

export class Notifier {
  name: string;
  on: boolean;
  fields: [{ name: string }];
  alerts: [Alert];
}

export class Settings {
  settings: {
    notifiers: [Notifier];
  };
}
