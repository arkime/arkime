export class Parliament {
  version: string;
  groups: [Object];
}

export class Group {
  title: string;
  description: string;
  clusters: [Object];
}

// TODO combine GroupCreated/ClusterCreated and Response?
export class GroupCreated {
  group: Object;
  success: boolean;
  text: string;
}

export class ClusterCreated {
  cluster: Object;
  succes: boolean;
  text: string;
  parliament: Parliament;
}

export class Response {
  success: boolean;
  text: string;
}
