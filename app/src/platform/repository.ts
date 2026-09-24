// Native: readings persist in SQLite. Metro picks repository.web.ts on web.
import { Repository } from '../store/Repository';
import { SqliteRepository } from '../store/SqliteRepository';

export function createRepository(): Repository {
  return new SqliteRepository();
}
