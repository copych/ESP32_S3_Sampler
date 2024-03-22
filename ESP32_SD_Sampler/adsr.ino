#include "adsr.h"
#include <math.h>

void Adsr::Init(float sample_rate, int blockSize)
{
    sample_rate_  = sample_rate / blockSize;
    attackShape_  = -1.f;
    attackTarget_ = 0.0f;
    attackTime_   = -1.f;
    decayTime_    = -1.f;
    releaseTime_  = -1.f;
    sus_level_    = 0.7f;
    x_            = 0.0f;
    gate_         = false;
    mode_         = ADSR_SEG_IDLE;

    SetTime(ADSR_SEG_ATTACK, 0.001f);
    SetTime(ADSR_SEG_DECAY, 0.1f);
    SetTime(ADSR_SEG_RELEASE, 0.05f);
}

void Adsr::Retrigger(bool hard)
{
    gate_ = true;
    mode_ = ADSR_SEG_ATTACK;
    if(hard)
	{
		x_ = 0.f;
		D0_ = attackD0_;
	}	
}

void Adsr::End(bool hard)
{
	gate_   = false;
	target_ = -0.001f;
	if(hard)
	{
		mode_ 	= ADSR_SEG_IDLE;
		D0_ 	  = attackD0_;
		x_ 		  = 0.f;
	} 
	else 
	{
		mode_ 	= ADSR_SEG_RELEASE;
		D0_ 	  = releaseD0_;
	} 
}

inline uint8_t Adsr::GetCurrentSegment() {
	uint8_t ret = mode_;
	if (gate_ && (x_ == sus_level_)) {
		ret = ADSR_SEG_SUSTAIN;
	}
	return ret;
}

void Adsr::SetTime(int seg, float time)
{
    switch(seg)
    {
        case ADSR_SEG_ATTACK: SetAttackTime(time, 0.0f); break;
        case ADSR_SEG_DECAY:
        {
            SetTimeConstant(time, decayTime_, decayD0_);
        }
        break;
        case ADSR_SEG_RELEASE:
        {
            SetTimeConstant(time, releaseTime_, releaseD0_);
        }
        break;
        default: return;
    }
}

void Adsr::SetAttackTime(float timeInS, float shape)
{
    if((timeInS != attackTime_) || (shape != attackShape_))
    {
        attackTime_  = timeInS;
        attackShape_ = shape;
        if(timeInS > 0.f)
        {
            float x         = shape;
            float target    = 9.f * powf(x, 10.f) + 0.3f * x + 1.01f;
            attackTarget_   = target;
            float logTarget = logf(1.f - (1.f / target)); // -1 for decay
            attackD0_       = 1.f - expf(logTarget / (timeInS * sample_rate_));
        }
        else
            attackD0_ = 1.f; // instant change
    }
}
void Adsr::SetDecayTime(float timeInS)
{
    SetTimeConstant(timeInS, decayTime_, decayD0_);
}
void Adsr::SetReleaseTime(float timeInS)
{
    SetTimeConstant(timeInS, releaseTime_, releaseD0_);
}


void Adsr::SetTimeConstant(float timeInS, float& time, float& coeff)
{
    if(timeInS != time)
    {
        time = timeInS;
        if(time > 0.f)
        {
            const float target = -0.43429448190325; // log(1/e)
            coeff              = 1.f - expf(target / (time * sample_rate_));
        }
        else
            coeff = 1.f; // instant change
    }
}


float Adsr::Process()
{
    float out = 0.0f;
    switch(mode_)
    {
        case ADSR_SEG_IDLE:
			out = 0.0f; 
			break;
        case ADSR_SEG_ATTACK:
            x_ += (float)D0_ * ((float)attackTarget_ - (float)x_);
            out = x_;
            if(out > 1.f)
            {
                mode_    = ADSR_SEG_DECAY;
                x_ = out = 1.f;
				target_  = sus_level_;
				D0_      = decayD0_;
            }
            break;
        case ADSR_SEG_DECAY:
        case ADSR_SEG_RELEASE:
            x_ += (float)D0_ * ((float)target_ - (float)x_);
            out = x_;
            if(out < 0.0f)
            {
                mode_    = ADSR_SEG_IDLE;
                x_ = out = 0.f;
				target_  = -0.001f;
				D0_      = attackD0_;
            }
			break;
        default: break;
    }
    return out;
}
