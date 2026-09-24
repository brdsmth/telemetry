// Web: expo-sqlite's worker needs Metro configuration the dev server lacks,
// and the web target is a demo surface, so state lives in memory for a tab.
import { MemoryRepository } from '../store/MemoryRepository';
import { Repository } from '../store/Repository';

export function createRepository(): Repository {
  return new MemoryRepository();
}
