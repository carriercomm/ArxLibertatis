/*
 * Copyright 2014 Arx Libertatis Team (see the AUTHORS file)
 *
 * This file is part of Arx Libertatis.
 *
 * Arx Libertatis is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Arx Libertatis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Arx Libertatis.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "game/magic/spells/SpellsLvl10.h"

#include "core/Application.h"
#include "core/Config.h"
#include "core/GameTime.h"
#include "game/Damage.h"
#include "game/Entity.h"
#include "game/EntityManager.h"
#include "game/NPC.h"
#include "game/Player.h"
#include "game/Spells.h"
#include "game/effect/Quake.h"
#include "game/spell/Cheat.h"

#include "graphics/particle/ParticleEffects.h"

#include "graphics/spells/Spells10.h"

#include "graphics/Draw.h"
#include "graphics/Renderer.h"

#include "scene/GameSound.h"
#include "scene/Interactive.h"

extern Rect g_size;

void MassLightningStrikeSpellLaunch(long i, SpellType typ)
{
	for(size_t ii = 0; ii < MAX_SPELLS; ii++) {
		if(spells[ii].exist && spells[ii].type == typ) {
			lightHandleDestroy(spells[ii].longinfo_light);
			spells[ii].tolive = 0;
		}
	}
	
	spells[i].exist = true;
	spells[i].lastupdate = spells[i].timcreation = (unsigned long)(arxtime);
	spells[i].tolive = 5000; // TODO probably never read
	spells[i].siz = 0;
	
	spells[i].longinfo_light = GetFreeDynLight();
	if(lightHandleIsValid(spells[i].longinfo_light)) {
		EERIE_LIGHT * light = lightHandleGet(spells[i].longinfo_light);
		
		light->intensity = 1.8f;
		light->fallend = 450.f;
		light->fallstart = 380.f;
		light->rgb = Color3f(1.f, 0.75f, 0.75f);
		light->pos = spells[i].vsource;
	}
	
	long count = std::max(long(spells[i].caster_level), 1l);
	CMassLightning * effect = new CMassLightning(count);
	effect->spellinstance=i;
	
	Vec3f target;
	float beta;
	if(spells[i].caster == 0) {
		target = player.pos + Vec3f(0.f, 150.f, 0.f);
		beta = player.angle.getPitch();
	} else {
		Entity * io = entities[spells[i].caster];
		target = io->pos + Vec3f(0.f, -20.f, 0.f);
		beta = io->angle.getPitch();
	}
	target.x -= std::sin(radians(MAKEANGLE(beta))) * 500.f;
	target.z += std::cos(radians(MAKEANGLE(beta))) * 500.f;
	
	effect->SetDuration(long(500 * spells[i].caster_level));
	effect->Create(target, MAKEANGLE(player.angle.getPitch()));
	spells[i].pSpellFx = effect;
	spells[i].tolive = effect->GetDuration();
	
	ARX_SOUND_PlaySFX(SND_SPELL_LIGHTNING_START);
	spells[i].snd_loop = ARX_SOUND_PlaySFX(SND_SPELL_LIGHTNING_LOOP, &target,
	                                       1.f, ARX_SOUND_PLAY_LOOPED);
	
	// Draws White Flash on Screen
	GRenderer->SetBlendFunc(Renderer::BlendOne, Renderer::BlendOne);
	GRenderer->SetRenderState(Renderer::AlphaBlending, true);
	EERIEDrawBitmap(Rectf(g_size), 0.00009f, NULL, Color::white);
	GRenderer->SetRenderState(Renderer::AlphaBlending, false);
}

void MassLightningStrikeSpellKill(long i)
{
	if(lightHandleIsValid(spells[i].longinfo_light)) {
		EERIE_LIGHT * light = lightHandleGet(spells[i].longinfo_light);
		
		light->duration = 200;
		light->time_creation = (unsigned long)(arxtime);
	}
	spells[i].longinfo_light = -1;
	
	ARX_SOUND_Stop(spells[i].snd_loop);
	ARX_SOUND_PlaySFX(SND_SPELL_LIGHTNING_END);
}

bool ControlTargetSpellLaunch(long i)
{
	if(!ValidIONum(spells[i].target)) {
		return false;
	}
	
	long tcount = 0;
	for(size_t ii = 1; ii < entities.size(); ii++) {
		
		Entity * ioo = entities[ii];
		if(!ioo || !(ioo->ioflags & IO_NPC)) {
			continue;
		}
		
		if(ioo->_npcdata->life <= 0.f || ioo->show != SHOW_FLAG_IN_SCENE) {
			continue;
		}
		
		if(ioo->groups.find("demon") == ioo->groups.end()) {
			continue;
		}
		
		if(closerThan(ioo->pos, spells[i].caster_pos, 900.f)) {
			tcount++;
			std::ostringstream oss;
			oss << entities[spells[i].target]->idString();
			oss << ' ' << long(spells[i].caster_level);
			SendIOScriptEvent(ioo, SM_NULL, oss.str(), "npc_control");
		}
	}
	if(tcount == 0) {
		return false;
	}
	
	ARX_SOUND_PlaySFX(SND_SPELL_CONTROL_TARGET);
	
	spells[i].exist = true;
	spells[i].lastupdate = spells[i].timcreation = (unsigned long)(arxtime);
	spells[i].tolive = 1000;
	
	CControlTarget * effect = new CControlTarget();
	effect->spellinstance = i;
	effect->Create(player.pos, MAKEANGLE(player.angle.getPitch()));
	effect->SetDuration(spells[i].tolive);
	spells[i].pSpellFx = effect;
	
	return true;
}

extern float GLOBAL_SLOWDOWN;

void FreezeTimeSpellLaunch(long duration, long i)
{
	ARX_SOUND_PlaySFX(SND_SPELL_FREEZETIME);
	
	float max_slowdown = std::max(0.f, GLOBAL_SLOWDOWN - 0.01f);
	spells[i].siz = clamp(spells[i].caster_level * 0.08f, 0.f, max_slowdown);
	GLOBAL_SLOWDOWN -= spells[i].siz;
	
	spells[i].exist = true;
	spells[i].tolive = (duration > -1) ? duration : 200000;
	spells[i].bDuration = true;
	spells[i].fManaCostPerSecond = 30.f * spells[i].siz;
	spells[i].longinfo_time = (long)arxtime.get_updated();
}

void FreezeTimeSpellEnd(size_t i)
{
	GLOBAL_SLOWDOWN += spells[i].siz;
	ARX_SOUND_PlaySFX(SND_SPELL_TELEKINESIS_END, &entities[spells[i].caster]->pos);
}

void MassIncinerateSpellLaunch(long i)
{
	ARX_SOUND_PlaySFX(SND_SPELL_MASS_INCINERATE);
	
	spells[i].exist = true;
	spells[i].lastupdate = spells[i].timcreation = (unsigned long)(arxtime);
	spells[i].tolive = 20000;
	
	long nb_targets=0;
	for(size_t ii = 0; ii < entities.size(); ii++) {
		
		Entity * tio = entities[ii];
		if(long(ii) == spells[i].caster || !tio || !(tio->ioflags & IO_NPC)) {
			continue;
		}
		
		if(tio->_npcdata->life <= 0.f || tio->show != SHOW_FLAG_IN_SCENE) {
			continue;
		}
		
		if(fartherThan(tio->pos, entities[spells[i].caster]->pos, 500.f)) {
			continue;
		}
		
		tio->sfx_flag |= SFX_TYPE_YLSIDE_DEATH | SFX_TYPE_INCINERATE;
		tio->sfx_time = (unsigned long)(arxtime);
		nb_targets++;
		ARX_SPELLS_AddSpellOn(ii, i);
	}
	
	if(nb_targets) {
		spells[i].snd_loop = ARX_SOUND_PlaySFX(SND_SPELL_INCINERATE_LOOP, 
		                                       &spells[i].caster_pos, 1.f, 
		                                       ARX_SOUND_PLAY_LOOPED);
	} else {
		spells[i].snd_loop = -1;
	}
}

void ARX_SPELLS_RemoveMultiSpellOn(long spell_id) {
	for(size_t i = 0; i < entities.size(); i++) {
		ARX_SPELLS_RemoveSpellOn(i, spells[spell_id].type);
	}
}

void MassIncinerateSpellEnd(size_t i)
{
	ARX_SPELLS_RemoveMultiSpellOn(i);
	ARX_SOUND_Stop(spells[i].snd_loop);
	ARX_SOUND_PlaySFX(SND_SPELL_INCINERATE_END);
}

extern float LASTTELEPORT;

void TeleportSpellLaunch(long i)
{
	spells[i].exist = true;
	spells[i].tolive = 7000;
	
	ARX_SOUND_PlaySFX(SND_SPELL_TELEPORT, &spells[i].caster_pos);
	
	if(spells[i].caster == 0) {
		LASTTELEPORT = 0.f;
	}
}

void TeleportSpellEnd(size_t i)
{
	ARX_SOUND_PlaySFX(SND_MAGIC_FIZZLE, &spells[i].caster_pos);
}