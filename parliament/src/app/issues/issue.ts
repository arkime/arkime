export class Issue {
  type: string;
  title: string;
  node: string;
  value: any;
  severity: string;
  groupId?: number;
  clusterId?: number;
  dismissed?: number;
  ignoreUntil?: number;
}
