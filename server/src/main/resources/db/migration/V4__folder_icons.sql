ALTER TABLE folders
  ADD COLUMN icon TEXT NOT NULL DEFAULT 'i-lucide-folder';

UPDATE folders
SET icon = CASE template_type
  WHEN 'OUTDOOR' THEN 'i-lucide-trees'
  WHEN 'BUILDING' THEN 'i-lucide-building-2'
  WHEN 'FLOOR' THEN 'i-lucide-layers-3'
  WHEN 'CORRIDOR' THEN 'i-lucide-route'
  WHEN 'ROOM' THEN 'i-lucide-door-open'
  ELSE 'i-lucide-folder'
END;

ALTER TABLE folders
  ADD CONSTRAINT ck_folders_icon_format
  CHECK (icon ~ '^i-lucide-[a-z0-9-]+$' AND length(icon) <= 100);
