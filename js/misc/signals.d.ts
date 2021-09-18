export type EventId = number;

export class EventEmitter {
  connect(event: string, handler: (...args: any[]) => any): EventId;
  disconnect(id: EventId);
  emit(event: string, ...args: any[]);
  disconnectAll();
  signalHandlerIsConnected(event: string);
}
