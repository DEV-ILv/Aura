-- =============================================================================
-- AURA Companion — initial Supabase schema (corrected)
-- =============================================================================
-- Run this from the Supabase SQL editor or via the Supabase CLI:
--
--     supabase db push
--
-- This migration creates the seven application tables, enables Row Level
-- Security (RLS) on every table, and adds per-user RLS policies so that each
-- authenticated user can only read/write their own rows.
--
-- TYPE CORRECTNESS (fixes `operator does not exist: uuid = bigint`):
--   * Every primary key `id` is UUID.
--   * Every `user_id` column is UUID and references auth.users (id) — UUID.
--   * Every RLS policy compares auth.uid() (UUID) against a UUID column.
--
--   If a previous attempt (or the Supabase Table Editor) created any of these
--   tables with BIGINT `id`/`user_id` columns, `CREATE TABLE IF NOT EXISTS`
--   would silently keep them and CREATE POLICY would then fail with
--   `uuid = bigint`. To make that impossible, the tables are dropped and
--   recreated below so the column types are always correct. This is safe here:
--   the schema is new and contains no production data. Use DROP with care on
--   an environment that already holds data.
--
-- Security model: every table carries a `user_id` column that is compared
-- against `auth.uid()`. The publishable (anon) key used by the Flutter app
-- therefore has no special privileges — RLS is the only guard between users.
-- The service role key can bypass RLS and MUST NEVER be shipped in the app.
-- =============================================================================

create extension if not exists "pgcrypto";

-- -----------------------------------------------------------------------------
-- Clean slate: remove any stale tables from a partial/editor-created run so
-- the column types below are guaranteed. Order matters because of FKs.
-- -----------------------------------------------------------------------------
drop table if exists public.settings      cascade;
drop table if exists public.notifications cascade;
drop table if exists public.memory        cascade;
drop table if exists public.reminders     cascade;
drop table if exists public.commands      cascade;
drop table if exists public.devices       cascade;
drop table if exists public.users         cascade;

-- -----------------------------------------------------------------------------
-- Helper triggers
-- -----------------------------------------------------------------------------

-- Keeps `updated_at` fresh on any table that has the column.
create or replace function public.set_updated_at()
returns trigger
language plpgsql
as $$
begin
  new.updated_at = now();
  return new;
end;
$$;

-- Auto-creates a public.users profile whenever a new auth user signs up.
create or replace function public.handle_new_user()
returns trigger
language plpgsql
security definer
set search_path = public
as $$
begin
  insert into public.users (id, email, display_name)
  values (
    new.id,
    new.email,
    coalesce(split_part(new.email, '@', 1), 'AURA user')
  )
  on conflict (id) do nothing;
  return new;
end;
$$;

-- =============================================================================
-- 1. users — profile mirror of auth.users
--    id        uuid   <- matches auth.users.id
-- =============================================================================
-- `id` defaults to auth.uid() so RLS-scoped inserts (WITH CHECK auth.uid()=id)
-- succeed even when the client omits the column, matching the app's behavior.
create table public.users (
  id           uuid primary key default auth.uid() references auth.users (id) on delete cascade,
  email        text unique,
  display_name text,
  created_at   timestamptz not null default now(),
  updated_at   timestamptz not null default now()
);

alter table public.users enable row level security;

create policy "users can read own profile"
  on public.users for select
  using (auth.uid() = id);

create policy "users can update own profile"
  on public.users for update
  using (auth.uid() = id);

create policy "users can insert own profile"
  on public.users for insert
  with check (auth.uid() = id);

-- =============================================================================
-- 2. devices — AURA hardware registered to an account
--    id        uuid
--    user_id   uuid   references auth.users (id)
-- =============================================================================
-- user_id defaults to auth.uid() so the companion can INSERT/upsert without
-- sending the column; RLS (WITH CHECK auth.uid()=user_id) still enforces the
-- owner. Without the default every app insert fails with 42501.
create table public.devices (
  id               uuid primary key default gen_random_uuid(),
  user_id          uuid not null default auth.uid() references auth.users (id) on delete cascade,
  device_id        text not null,            -- unique serial / MAC of the ESP32
  name             text not null,
  model            text not null default '',
  firmware_version text not null default '',
  mark             text not null default '',
  codename         text not null default '',
  channel          text not null default '',
  is_online        boolean not null default false,
  last_seen_at     timestamptz,
  created_at       timestamptz not null default now(),
  updated_at       timestamptz not null default now(),
  unique (user_id, device_id)
);

alter table public.devices enable row level security;

create policy "devices select own"
  on public.devices for select
  using (auth.uid() = user_id);

create policy "devices insert own"
  on public.devices for insert
  with check (auth.uid() = user_id);

create policy "devices update own"
  on public.devices for update
  using (auth.uid() = user_id);

create policy "devices delete own"
  on public.devices for delete
  using (auth.uid() = user_id);

-- =============================================================================
-- 3. commands — audit log of commands issued to a device
--    id        uuid
--    user_id   uuid   references auth.users (id)
-- =============================================================================
create table public.commands (
  id         uuid primary key default gen_random_uuid(),
  user_id    uuid not null default auth.uid() references auth.users (id) on delete cascade,
  device_id  text not null default '',
  command    text not null,
  args       jsonb,
  status     text not null default 'sent',   -- sent | success | error
  response   text,
  created_at timestamptz not null default now()
);

alter table public.commands enable row level security;

create policy "commands select own"
  on public.commands for select
  using (auth.uid() = user_id);

create policy "commands insert own"
  on public.commands for insert
  with check (auth.uid() = user_id);

create policy "commands update own"
  on public.commands for update
  using (auth.uid() = user_id);

create policy "commands delete own"
  on public.commands for delete
  using (auth.uid() = user_id);

-- =============================================================================
-- 4. reminders — cloud-synced reminders
--    id        uuid
--    user_id   uuid   references auth.users (id)
-- =============================================================================
create table public.reminders (
  id              uuid primary key default gen_random_uuid(),
  user_id         uuid not null default auth.uid() references auth.users (id) on delete cascade,
  title           text not null,
  body            text not null default '',
  remind_at       timestamptz,
  repeat_interval text not null default 'once',  -- once | daily | weekly
  is_completed    boolean not null default false,
  created_at      timestamptz not null default now(),
  updated_at      timestamptz not null default now()
);

alter table public.reminders enable row level security;

create policy "reminders select own"
  on public.reminders for select
  using (auth.uid() = user_id);

create policy "reminders insert own"
  on public.reminders for insert
  with check (auth.uid() = user_id);

create policy "reminders update own"
  on public.reminders for update
  using (auth.uid() = user_id);

create policy "reminders delete own"
  on public.reminders for delete
  using (auth.uid() = user_id);

-- =============================================================================
-- 5. memory — durable memories per user
--    id        uuid
--    user_id   uuid   references auth.users (id)
-- =============================================================================
create table public.memory (
  id         uuid primary key default gen_random_uuid(),
  user_id    uuid not null default auth.uid() references auth.users (id) on delete cascade,
  content    text not null,
  category   text not null default 'general',
  source     text not null default 'companion',  -- companion | device
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);

alter table public.memory enable row level security;

create policy "memory select own"
  on public.memory for select
  using (auth.uid() = user_id);

create policy "memory insert own"
  on public.memory for insert
  with check (auth.uid() = user_id);

create policy "memory update own"
  on public.memory for update
  using (auth.uid() = user_id);

create policy "memory delete own"
  on public.memory for delete
  using (auth.uid() = user_id);

-- =============================================================================
-- 6. notifications — notification history per user
--    id        uuid
--    user_id   uuid   references auth.users (id)
-- =============================================================================
create table public.notifications (
  id         uuid primary key default gen_random_uuid(),
  user_id    uuid not null default auth.uid() references auth.users (id) on delete cascade,
  device_id  text not null default '',
  title      text not null,
  body       text not null default '',
  type       text not null default 'device',    -- device | reminder | system
  read       boolean not null default false,
  created_at timestamptz not null default now()
);

alter table public.notifications enable row level security;

create policy "notifications select own"
  on public.notifications for select
  using (auth.uid() = user_id);

create policy "notifications insert own"
  on public.notifications for insert
  with check (auth.uid() = user_id);

create policy "notifications update own"
  on public.notifications for update
  using (auth.uid() = user_id);

create policy "notifications delete own"
  on public.notifications for delete
  using (auth.uid() = user_id);

-- =============================================================================
-- 7. settings — per-user key/value settings (value stored as JSONB)
--    id        uuid
--    user_id   uuid   references auth.users (id)
-- =============================================================================
create table public.settings (
  id         uuid primary key default gen_random_uuid(),
  user_id    uuid not null default auth.uid() references auth.users (id) on delete cascade,
  key        text not null,
  value      jsonb,
  updated_at timestamptz not null default now(),
  unique (user_id, key)
);

alter table public.settings enable row level security;

create policy "settings select own"
  on public.settings for select
  using (auth.uid() = user_id);

create policy "settings insert own"
  on public.settings for insert
  with check (auth.uid() = user_id);

create policy "settings update own"
  on public.settings for update
  using (auth.uid() = user_id);

create policy "settings delete own"
  on public.settings for delete
  using (auth.uid() = user_id);

-- =============================================================================
-- Triggers
-- =============================================================================

-- Create a profile row for every new auth user (idempotent on re-runs).
drop trigger if exists on_auth_user_created on auth.users;
create trigger on_auth_user_created
  after insert on auth.users
  for each row execute function public.handle_new_user();

-- Keep updated_at columns in sync (idempotent on re-runs).
drop trigger if exists set_updated_at on public.users;
create trigger set_updated_at
  before update on public.users
  for each row execute function public.set_updated_at();

drop trigger if exists set_updated_at on public.devices;
create trigger set_updated_at
  before update on public.devices
  for each row execute function public.set_updated_at();

drop trigger if exists set_updated_at on public.reminders;
create trigger set_updated_at
  before update on public.reminders
  for each row execute function public.set_updated_at();

drop trigger if exists set_updated_at on public.memory;
create trigger set_updated_at
  before update on public.memory
  for each row execute function public.set_updated_at();

drop trigger if exists set_updated_at on public.settings;
create trigger set_updated_at
  before update on public.settings
  for each row execute function public.set_updated_at();

-- =============================================================================
-- Indexes (RLS filters on user_id benefit from these)
-- =============================================================================
create index if not exists idx_devices_user_id       on public.devices (user_id);
create index if not exists idx_commands_user_id      on public.commands (user_id);
create index if not exists idx_reminders_user_id     on public.reminders (user_id);
create index if not exists idx_reminders_remind_at   on public.reminders (remind_at);
create index if not exists idx_memory_user_id        on public.memory (user_id);
create index if not exists idx_notifications_user_id on public.notifications (user_id);
create index if not exists idx_settings_user_id      on public.settings (user_id);

-- =============================================================================
-- Grants
-- =============================================================================
-- IMPORTANT: dropping and recreating the tables removes the privileges that
-- Supabase normally applies to new tables. Without these GRANTs, PostgREST
-- returns `42501 permission denied` for the anon/authenticated roles, which
-- blocks the app's cloud mode even though RLS is enabled. GRANT is idempotent
-- and safe to re-run.
--
--   anon           -> publishable key, no session (RLS hides all rows)
--   authenticated  -> signed-in users (RLS allows only their own rows)
--   service_role   -> server-side admin (never used by the app)
--
-- RLS remains the security boundary; these grants only let requests through
-- so RLS can evaluate the policies.
grant all on table public.users         to anon, authenticated, service_role;
grant all on table public.devices       to anon, authenticated, service_role;
grant all on table public.commands      to anon, authenticated, service_role;
grant all on table public.reminders     to anon, authenticated, service_role;
grant all on table public.memory        to anon, authenticated, service_role;
grant all on table public.notifications to anon, authenticated, service_role;
grant all on table public.settings      to anon, authenticated, service_role;
