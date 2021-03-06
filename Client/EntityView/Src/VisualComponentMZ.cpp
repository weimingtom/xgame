//==========================================================================
/**
* @file	  : VisualComponentMZ.cpp
* @author : 
* created : 2008-1-30   12:57
*/
//==========================================================================

#include "stdafx.h"
#include "VisualComponentMZ.h"
#include "IGlobalClient.h"
#include "IResourceManager.h"
#include "ISceneManager2.h"
#include "MoveComponentMZ.h"
#include "StateDef.h"
#include "IEntityFactory.h"
#include "MWDLoader.h"
#include "PartManager.h"
#include "PetManager.h"
#include "IAudioEngine.h"
#include "IFormManager.h"
#include "ConfigActionMap.h"
//#include "ICollectionClient.h"
#include "IConfigManager.h"
#include "ITeamClient.h"
#include "../../../Base/EventEngine/Include/IEventEngine.h"


#define _32_X_sqrt_2	45.254834f	// 32与根号2的乘积，3D坐标中，一个TILE的宽度（0方向）为这个值
#define StockStepLength		90.f	// 假设移动时的播放速度为1时的步长为10


//  这里暂时将移动速度设置为走动，因为现有实现中，部分生物没有提供“Run”的动画；
//  注意：这里对m_ulMoveSpeed的设置需要与对VisualComponent::mIsRun的设置相匹配；
VisualComponentMZ::VisualComponentMZ()
:mShadowManager(0),
m_pJumpManager(NULL),
m_bUseBigTexture(false)
{
	//在这里构建状态管理器
	m_stateMgr.Create(this);

	//初始化骑乘管理器
	mRideManager.create(this,this);

	//初始化换装管理器
	mChangePartManager.create(this,this);

}

VisualComponentMZ::~VisualComponentMZ()
{
	close();
}

void VisualComponentMZ::create()
{
	getOwner()->setFlag(flagSelectable | flag3D | flagDrawName | flagInScreen | flagFade);//添加死亡状态
	mModifierList.setOwner(getOwner());
	//Info("create-mz:"<<_fi("0x%08x", (ulong)getOwner())<<endl);
}

void VisualComponentMZ::close()
{
	getOwner()->removeFlag(flagFade);
	onLeaveViewport();
}

size_t VisualComponentMZ::onLoad(Stream* stream, size_t len)
{
	char name[256];
	uchar str_len;
	if (len == 0) return 0;

	stream->read(&str_len, sizeof(uchar));
	stream->read(name, str_len);
	name[str_len] = 0;
	int creatureId = *(int*)name;
	getOwner()->setResId(creatureId);

	//旧的实现
	//mConfig = ConfigCreatures::Instance()->getCreature(getOwner()->getResId());
	//if (!mConfig)
	//	return 0;
	//新的实现
	mModelNodeState.m_pConfig = ConfigCreatures::Instance()->getCreature(getOwner()->getResId());
	if( !mModelNodeState.m_pConfig)
		return 0;

    mShowRect.left = mModelNodeState.m_pConfig->boundingBox.left;
    mShowRect.top = mModelNodeState.m_pConfig->boundingBox.top;
    mShowRect.right = mModelNodeState.m_pConfig->boundingBox.right - 1;
    mShowRect.bottom = mModelNodeState.m_pConfig->boundingBox.bottom - 1;

    return str_len + sizeof(uchar);
}

size_t VisualComponentMZ::onSave(Stream* stream) const
{
	uchar str_len = sizeof(int);//(uchar)resId.length();
	stream->write(&str_len, sizeof(uchar));
	int creatureId = getOwner()->getResId();
	stream->write(&creatureId, str_len);
	return str_len + sizeof(uchar);
}

void VisualComponentMZ::onRemoveEntity()
{
	VisualComponent::onRemoveEntity();
	getOwner()->onCommand(EntityCommand_Stand); // 防止对象移除了，移动还在继续
}

const AABB& VisualComponentMZ::GetAABB()
{
	ModelNode *pMode = getModelNode();
	if (pMode)
	{
		getOwner()->m_AABB = pMode->getWorldAABB();
	}
	return getOwner()->m_AABB;
}

const Matrix4& VisualComponentMZ::GetFullTransform()
{
	ModelNode *pMode = getModelNode();
	if (pMode)
	{
		getOwner()->m_FullTransform = pMode->getFullTransform();
	}
	return getOwner()->m_FullTransform;
}


void VisualComponentMZ::onEnterViewport(int priority)
{
	requestRes(priority);	

	if (getOwner()->hasFlag(flagFade|flagInScreen))
		setFadeIn();

	mAttachObjHolder.onEnterViewport();
}

void VisualComponentMZ::onLeaveViewport()
{
	//mHandle永远无效
	//if (isValidHandle(mHandle))
	//{
		if (getOwner()->hasFlag(flagFade|flagInScreen))
			setFadeOut();
		else
			releaseRes();

	//}
	mAttachObjHolder.onLeaveViewport();
}

void VisualComponentMZ::requestRes(int priority)
{
	//旧的实现
	//if (!isValidHandle(mHandle))
	//{
	//	if (getOwner()->getUserData() != 0)
	//		priority -= 2;
	//	if (getOwner()->isMainPlayer())
	//		priority--;

	//	Assert(getOwner()->getResId() != 0);
	//	mNewResId = MZIDMap::getInstance().add(getOwner()->getResId(), 0);
	//	mHandle = gGlobalClient->getResourceManager()->requestResource(mNewResId, 
	//		(ResourceType)getOwner()->getResType(), true /*false*/, priority);
	//	m_pBaseNode = getModelNode();
	//}

	//新的实现
	mChangePartManager.setCanRequestRes(true);
	mChangePartManager.requestRes( priority);
	mRideManager.setCanRequestRes(true);
	mRideManager.requestRes(priority);
}

void VisualComponentMZ::releaseRes()
{	
	//旧的实现
	//if (isValidHandle(mHandle))
	//{
	//	mModifierList.unmodify();
	//	gGlobalClient->getResourceManager()->releaseResource(mHandle);
	//	MZIDMap::getInstance().remove(mNewResId);
	//	mResLoaded = false;
	//	mHandle = INVALID_HANDLE;
	//}
	//m_pBaseNode = 0;

	//新的实现
	mModifierList.unmodify();
	mChangePartManager.setCanRequestRes(false);
	mChangePartManager.releaeRes();
	mRideManager.setCanRequestRes(false);
	mRideManager.releaseRes();
}

//void VisualComponentMZ::onResLoaded(ModelNode* node)
//{
//	ModelInstance* inst = node->getModelInstance();
//	if (inst)
//	{
//		mResLoaded = true;
//		inst->setCallback(static_cast<IAnimationCallback*>(this));
//		if (getOwner()->getEntityType() == typeEffect)
//			setAnimate("Birth", 0);
//		else
//			setAction(GetSaveEntityAction(), -1,0, true);
//			//setAction(EntityAction_Stand, -1,0, true);
//
//		mUpdateOption |= updateAngle;
//
//		//ulong moveSpeed = m_ulTilePerTile;//_32_X_sqrt_2 / m_ulTilePerTile; // 计算初始移动速度
//		getOwner()->onMessage(msgMoveSpeedChanged, m_ulTilePerTile);
//
//		//modify by yhc 走和跑的动画播放速度不变
//		//mAniSpeedMove = StockStepLength / mConfig->stepLength; // 计算初始移动播放速度
//
//
//		mUpdateOption |= updateScale;
//		//if (getOwner()->isMainPlayer())
//		//Info("Scale: "<<_ff("%.2f", mScale)<<",AniSpeed: "<<_ff("%.2f", mAniSpeedMove)<<",TileMoveTime: "<<m_ulMoveSpeed<<",MoveSpeed: "<<_ff("%.2f", moveSpeed)<<endl);
//
//		// 装载部件
//		if (mUpdateOption & updatePart)
//		{
//			PartManager::getInstance().apply(getOwner(), EntityCommand_SetPart);
//			//PartManager::getInstance().apply(getOwner());
//			mUpdateOption &= ~updatePart;
//		}
//
//		// 装载部件
//		if (mUpdateOption & updateRidePet)
//		{
//			PetManager::getInstance().Apply(getOwner());
//			mUpdateOption &= ~updateRidePet;
//		}
//
//		// 由于异步加载，所以当模型加载完后要设置一下当前动画
//		const ActionInfo& info = mActionList.getCurActionInfo();
//		setAction(info.actionId, info.loops);
//		//setAnimate(getActionName(info.actionId), 0, info.loops);
//		mUpdateOption |= updatePosition;
//		mAttachObjHolder.setPosition(getOwner()->getSpace());
//
//		// 重设一下showRect
//		mConfig = ConfigCreatures::Instance()->getCreature(getOwner()->getResId());
//		if (mConfig)
//		{
//			mShowRect.left = mConfig->boundingBox.left;
//			mShowRect.top = mConfig->boundingBox.top;
//			mShowRect.right = mConfig->boundingBox.right - 1;
//			mShowRect.bottom = mConfig->boundingBox.bottom - 1;		
//		}
//	}
//
//	if (mShadowManager)
//	{
//			mShadowManager->setModelNode(node);
//	}
//}


void VisualComponentMZ::drawPickObject(IRenderSystem* pRenderSystem)
{
	// 屏蔽掉周边玩家模型的绘制
	if (ShieldAroundPlayer())
	{
		// 特效还是需要绘制出来的；
		mAttachObjHolder.draw(pRenderSystem);
		return;
	}
	bool bHighLight = getOwner()->hasFlag(flagHighlight);
	TextureStageOperator colorOP, alphaOP;
	TextureStageArgument colorArg0, colorArg1, alphaArg0, alphaArg1;
	ColorValue oldColor;
	if (bHighLight)
	{
		// 高亮显示 [4/18/2011 zgz]
		pRenderSystem->getTextureStageStatus(0, colorOP, colorArg0, colorArg1, alphaOP, alphaArg0, alphaArg1);
		pRenderSystem->setTextureStageStatus(0, TSO_MODELATE2X,  TSA_PREVIOUS, TSA_TEXTURE, alphaOP, alphaArg0, alphaArg1);
	}
	ModelNode* pNode = getModelNode();
	if (pNode)
	{
		pNode->render(pRenderSystem, true);
	}

	if (bHighLight)
	{
		pRenderSystem->setTextureStageStatus(0, colorOP,  colorArg0, colorArg1, alphaOP, alphaArg0, alphaArg1);
	}

		mAttachObjHolder.draw(pRenderSystem);

	RunType runType = gGlobalClient->getSceneManager()->getRunType();
	//// 能被采集的实体 需要播特效；
	IEntity* pEntity = (IEntity*)getOwner()->getUserData();
	if(pEntity  && runType == RUN_TYPE_GAME)
	{	
		ISchemeCenter* sc = gGlobalClient->getEntityClient()->GetSchemeCenter();
		ulong id = pEntity->GetNumProp(CREATURE_MONSTER_ID);
		SMonsterSchemeInfo* smsinfo = sc->GetMonsterSchemeInfo(id);
		if (!smsinfo)
		{
			return;
		}
		// NPCEFFECT 改成服务端完全控制；
		if (smsinfo->lType != MONSTER_TYPE_NPCCOLLECTION)
		{
			return;
		}

		IMonster* pMonster = (IMonster*) pEntity;
		if (pMonster)
		{
			pMonster->CreateCollectionEffect();
		}
	}
	return;
}

void VisualComponentMZ::drawShadow(IRenderSystem* pRenderSystem)
{
	ModelNode* node = getModelNode();
	if (node)
	{
		// 屏蔽掉周边玩家模型的绘制
		if (ShieldAroundPlayer())
		{
			return;
		}
		//if (getOwner()->hasFlag(flagFade))
		{
			if ((mUpdateOption & updateFadeIn) || (mUpdateOption & updateFadeOut))
			{
				ColorValue c = pRenderSystem->getColor();
				pRenderSystem->setColor(ColorValue(0.0,0.0,0.0, mAlpha*0.3));
				node->render(pRenderSystem, false);
				pRenderSystem->setColor(c);
			}
			else
			{
				node->render(pRenderSystem,false);
			}
		}
	}
}

void VisualComponentMZ::draw(IRenderSystem* pRenderSystem)
{
	if (mShadowManager)
	{
		ModelNode* pShadowNode = getModelNode();
		mShadowManager->render(pRenderSystem, pShadowNode);
		mAttachObjHolder.draw(pRenderSystem);
	}
	else
		drawPickObject(pRenderSystem);
}
int VisualComponentMZ::CheckAngleDir(long & lFrom,long & lTo)
{
	int nDir = 1;
	if( lTo > lFrom )
	{
		//目标角度大
		if( (lTo - lFrom) > 180 )
			nDir = -1;//减法
	}
	else
	{
		//目标角度小
		if( (lFrom - lTo) < 180 )
			nDir = -1;
	}
	return nDir;
}

bool VisualComponentMZ::update(float tick, float deltaTick, IRenderSystem* pRenderSystem)
{
	//更新父类
	VisualComponent::update( tick, deltaTick, pRenderSystem);


	//判断是否需要删除
	if (getOwner()->isNeedDelete()/* && mUpdateOption == 0*/)
	{
		//Info("delete-mz:"<<_fi("0x%08x", (ulong)getOwner())<<endl);
		//if (getOwner()->hasFlag(flagInMap))
		     //gGlobalClient->getSceneManager()->removeEntity(getOwner()->getTile(), getOwner());
		if (!getOwner()->hasFlag(flagFade) || (mUpdateOption & updateFadeOut) == 0)
		{
			getOwner()->removeFlag(flagFade);
			// zgz 安全起见 从场景管理里面去掉
			gGlobalClient->getSceneManager()->removeEntity(getOwner()->getTile(), getOwner());
			delete getOwner();
			return false;
		}
	}

	if(m_bUseBigTexture && mModelNodeState.m_pCompositeNode)
	{
		UseBigTexture();
		m_bUseBigTexture = false;
	}

	//设置需要更新的节点
	//换装管理器和骑乘管理器都会改变当前处理的节点
	mChangePartManager.update(); //换装管理器更新
	mRideManager.update();//骑乘管理器更新



	ColorValue baseColorValue(ColorValue::White);
	if (getOwner()->hasFlag(flagHighlight))
	{
		// 这里的颜色配置配合TSO_MODELATE2X，达到颜色*1.4倍数的效果
		baseColorValue.r = 0.7;
		baseColorValue.g = 0.7;
		baseColorValue.b = 0.7;
	}

	//add by yhc 跳跃控制
	//by yhc 7.5; 修正位置不同步问题,原因是屏幕外往内跳的时候没有做update
	if(mUpdateOption & updateJump)
	{
		if(m_pJumpManager)
		{
			JUMPSTATE jsold = m_pJumpManager->GetJumpState();
			m_pJumpManager->update(tick,deltaTick,getOwner());
			JUMPSTATE jsnow = m_pJumpManager->GetJumpState();
			// 
			bool bHero =false;
			IPerson* pHero = gGlobalClient->getEntityClient()->GetHero();
			IEntity* pEntity = (IEntity*)getOwner()->getUserData();
			if (pHero && pEntity && pEntity->GetUID() == pHero->GetUID())
			{
				bHero = true;
			}
			if(jsnow!=jsold)
			{
				if(jsnow==JUMPSTATE_STATR)
					setAction(EntityAction_JumpStart, 1);
				if(jsnow==JUMPSTATE_MIDWAY)
					setAction(EntityAction_JumpMidway, -1);
				if(jsnow==JUMPSTATE_END)
					setAction(EntityAction_JumpEnd, 1);
				// 得到跳跃状态(WZH)
				if (bHero && pHero && pHero->GetCurState() != CREATURE_CONTROLSTATE_JUMP)
				{
					pHero->GotoState(CREATURE_CONTROLSTATE_JUMP, NULL, 0);
				}
			}

			if( m_pJumpManager->GetJumpState() == JUMPSTATE_FINISH )
			{
				mUpdateOption &= ~updateJump;
				m_pJumpManager->JumpEnd(getOwner());
				delete m_pJumpManager;
				m_pJumpManager = NULL;
				Info("跳跃结束！");
				setAction(EntityAction_Stand,-1);
				// 这里是跳跃结束的标识；在人物那添加个属性;(WZH)
				// 得到跳跃状态；并改回站立状态
				if (bHero && pHero && pHero->GetCurState() == CREATURE_CONTROLSTATE_JUMP)
				{
					pHero->GotoState(CREATURE_CONTROLSTATE_STAND, NULL, 0);
					// 优先判断是否关闭交互窗口(在移动过程中进行判断)
					xs::Point ptMapLoc = pHero->GetMapLoc();
					IFormManager* pFromManager = gGlobalClient->getFormManager();
					if (pFromManager)
					{
						pFromManager->CloseTaskFormWinodow(ptMapLoc.x ,ptMapLoc.y,true);
					}
				}
			}
		}
	}
	
	if (mShadowManager) //待写
	{
		ModelNode* pShadowNode = getModelNode();
		mShadowManager->update(pShadowNode, (ulong)tick, (ulong)deltaTick);
	}

	//节点更新
	if (getOwner()->hasFlag(flagInScreen))
	{	
		ModelNode* node = getModelNode();
		if (node)
		{
			if (mUpdateOption)
			{
				if (mUpdateOption & updatePosition)
				{
						node->setPosition(getOwner()->getSpace());
						mUpdateOption &= ~updatePosition;
				}

				if (mUpdateOption & updateScale)
				{
					//判断是骑乘还是非骑乘
					if( mModelNodeState.m_pRideNode )//骑乘状态
					{
						if( mModelNodeState.m_pRideConfig )
						{
							node->setScale(mModelNodeState.m_pRideConfig->scale.x * mModelNodeState.m_fScale,
								mModelNodeState.m_pRideConfig->scale.y * mModelNodeState.m_fScale,
								mModelNodeState.m_pRideConfig->scale.z * mModelNodeState.m_fScale);
						}
					}
					else //非骑乘状态
					{
						if( mModelNodeState.m_pConfig)
						{
							node->setScale(mModelNodeState.m_pConfig->scale.x * mModelNodeState.m_fScale,
								mModelNodeState.m_pConfig->scale.y * mModelNodeState.m_fScale,
								mModelNodeState.m_pConfig->scale.z * mModelNodeState.m_fScale);
						}
					}

                    mShowRect.left = mModelNodeState.m_pConfig->boundingBox.left;
                    mShowRect.top = mModelNodeState.m_pConfig->boundingBox.top;
                    mShowRect.right = mModelNodeState.m_pConfig->boundingBox.right - 1;
                    mShowRect.bottom = mModelNodeState.m_pConfig->boundingBox.bottom - 1;
					mUpdateOption &= ~updateScale;
				}

				if (mUpdateOption & updateAniSpeed)
				{				
					node->getModelInstance()->setAnimationSpeed(mModelNodeState.m_fAniSpeed);
					mUpdateOption &= ~updateAniSpeed;
				}

				if (mUpdateOption & updateAngle)
				{
					if( mModelNodeState.m_lCurAngle == -1 )
						mModelNodeState.m_lCurAngle = getOwner()->getAngle();
					else
					{
						long lToAngle = getOwner()->getAngle();
						int nDir = CheckAngleDir(mModelNodeState.m_lCurAngle,lToAngle);
						
						mModelNodeState.m_lCurAngle += nDir* deltaTick;

						//add by yhc 
						//大家不至于被转晕了
						if(mModelNodeState.m_lCurAngle>360||mModelNodeState.m_lCurAngle<-360)
							mModelNodeState.m_lCurAngle = mModelNodeState.m_lCurAngle%360;

						if(mModelNodeState.m_lCurAngle>=360)
							mModelNodeState.m_lCurAngle -=360;
						if(mModelNodeState.m_lCurAngle<0)
							mModelNodeState.m_lCurAngle += 360;
							
						//转过头了
						if( nDir != CheckAngleDir(mModelNodeState.m_lCurAngle,lToAngle) )
							mModelNodeState.m_lCurAngle = lToAngle;
					}
					Quaternion q;
					q.FromAngleAxis(mModelNodeState.m_lCurAngle,Vector3(0,1,0));
					node->setOrientation(q);
					if( mModelNodeState.m_lCurAngle == getOwner()->getAngle())
						mUpdateOption &= ~updateAngle;
				}

				//已经被换装管理器取代
				//if (mUpdateOption & updateColor)
				//{
				//	node->setGlobalDiffuse(mModelNodeState.m_vColor);
				//	mUpdateOption &= ~updateColor;
				//}

				
				node->update(tick, deltaTick, pRenderSystem);

				if (mUpdateOption & updateFadeIn)
				{
					ColorValue clr(1.f, 1.f, 1.f, mAlpha);
					clr = clr * baseColorValue;
					node->updateColor(pRenderSystem, clr);

					mAlpha += FADE_ALPHA * deltaTick;
					if (mModelNodeState.m_bPetDie && mAlpha>0.3f)
					{
						mAlpha = 0.3f;
						mUpdateOption &=~updateFadeIn;
					}
					else if (mAlpha > 1.0f)
					{
						mAlpha = 1.0f;
						mUpdateOption &= ~updateFadeIn;
					}					
				}

			    if (mUpdateOption & updateFadeOut)
				{
					ColorValue clr(1.f, 1.f, 1.f, mAlpha);
					clr = clr * baseColorValue;
					node->updateColor(pRenderSystem, clr);
					mAlpha -= FADE_ALPHA * deltaTick;
					if (mAlpha < 0.f)
					{
						mAlpha = 0.f;
						mUpdateOption &= ~updateFadeOut;

						releaseRes();
						return true;
					}
				}

				if (mUpdateOption & updatePetdie)
				{
					ColorValue clr(1.f, 1.f, 1.f, mAlpha);
					clr = clr * baseColorValue;
					mAlpha = 0.3f;
					node->updateColor(pRenderSystem, clr);
					if (!mModelNodeState.m_bPetDie)
					{
						mUpdateOption &= ~updatePetdie;
					}
				}
			}
			else 
			{
				node->update(tick, deltaTick, pRenderSystem, baseColorValue);		
			}

			

			mAttachObjHolder.update(tick, deltaTick, pRenderSystem);
		}
	}
	else
	{
		if (mUpdateOption & updateFadeOut)
		{
			mUpdateOption &= ~updateFadeOut;
			releaseRes();
			return true;
		}
		else
		{
			breakable;
		}
		mAttachObjHolder.update(tick, deltaTick, pRenderSystem);

		ModelNode* node = getModelNode();
		if (node)
		{
			ModelInstance* inst = node->getModelInstance();
			inst->advanceTime(deltaTick);
		}
		else
		{
			breakable;
		}

	}
	return true;
}

void VisualComponentMZ::setAniSpeed(float speed)
{
	mModelNodeState.m_fAniSpeed = speed;
	mUpdateOption |=updateAniSpeed;
	return;
}

int VisualComponentMZ::GetAnimationTime(int nActID)
{
	ActionMapContext bodyContext;

	if( getOwner()->getEntityType() == typePerson )  
		bodyContext.iscreature = true;
	else 
		bodyContext.iscreature =false;

	if(mModelNodeState.m_pRideNode)
		bodyContext.ismount = true;
	else
		bodyContext.ismount = false;

	bodyContext.state = m_stateMgr.GetCurrentState();
	bodyContext.weapon = mChangePartManager.getWeaponClass();

	const char * pAction = ConfigActionMap::Instance()->getMappedAction(bodyContext, nActID);
	int nTicks = 0;
	if (mModelNodeState.m_pBodyNode == NULL)
	{
		WarningLn("模型不存在！");
		return 0;
	}
	nTicks = mModelNodeState.m_pBodyNode->getModelInstance()->getAnimationTicks(pAction);
	return nTicks;
}

int VisualComponentMZ::GetJumpTime(xs::Point ptSrc, xs::Point ptTarget)
{
	m_pJumpManager = new JumpManager();
	m_pJumpManager->create(ptSrc ,ptTarget);
	m_pJumpManager->SetMoveSpeed(_32_X_sqrt_2 / mModelNodeState.m_ulTilePerTile);

	return m_pJumpManager->GetAllTicks();
}

bool VisualComponentMZ::setAnimate()
{
	//设置骑乘节点的动作
	if( mModelNodeState.m_pRideNode )
	{
		ActionMapContext ridecontext;
		ridecontext.iscreature = false;
		ridecontext.ismount = false;
		ridecontext.state = m_stateMgr.GetCurrentState();
		ridecontext.weapon = EWeapon_NoWeapon;
		const char * pRideAction = ConfigActionMap::Instance()->getMappedAction(ridecontext,mModelNodeState.m_curActionId);
		bool ret = mModelNodeState.m_pRideNode->getModelInstance()->setCurrentAnimation(pRideAction,mModelNodeState.m_curActionLoops);
		if( !ret)
		{
			const char * pRideActionRep = ConfigActionMap::Instance()->getMappedAction(ridecontext,mModelNodeState.m_curRepActionId);
			bool ret2 = mModelNodeState.m_pRideNode->getModelInstance()->setCurrentAnimation(pRideActionRep, mModelNodeState.m_curActionLoops);
			if (!ret2)
				return false;
		}

		//设置动作速度
		mModelNodeState.m_pRideNode->getModelInstance()->setAnimationSpeed(mModelNodeState.m_fAniSpeed);
	}

	//设置身体节点动作
	if( mModelNodeState.m_pBodyNode)
	{
		ActionMapContext bodyContext;

		if( getOwner()->getEntityType() == typePerson )  
			bodyContext.iscreature = true;
		else 
			bodyContext.iscreature =false;

		if(mModelNodeState.m_pRideNode)
			bodyContext.ismount = true;
		else
			bodyContext.ismount = false;

		bodyContext.state = m_stateMgr.GetCurrentState();
		bodyContext.weapon = mChangePartManager.getWeaponClass();

		//如果骑乘节点配置了动作，就用骑乘节点的动作
		bool ret = false;
		if( bodyContext.ismount
			&& mModelNodeState.m_pRideConfig 
			&& !mModelNodeState.m_pRideConfig->cRide.rideAction.empty() )
		{
			std::string action = mModelNodeState.m_pRideConfig->cRide.rideAction;
			ret = mModelNodeState.m_pBodyNode->getModelInstance()->setCurrentAnimation(action,mModelNodeState.m_curActionLoops);
		}

		if( !ret )//当前的动作
		{
			const char * pAction = ConfigActionMap::Instance()->getMappedAction(bodyContext, mModelNodeState.m_curActionId);
			ret = mModelNodeState.m_pBodyNode->getModelInstance()->setCurrentAnimation(pAction, mModelNodeState.m_curActionLoops);
		}

		if( !ret)//备用动作
		{
			const char * pActionRep =  ConfigActionMap::Instance()->getMappedAction(bodyContext, mModelNodeState.m_curRepActionId);
			bool ret2 = mModelNodeState.m_pBodyNode->getModelInstance()->setCurrentAnimation(pActionRep, mModelNodeState.m_curActionLoops);
			if (!ret2)
				return false;
		}

		//设置动作速度
		mModelNodeState.m_pBodyNode->getModelInstance()->setAnimationSpeed(mModelNodeState.m_fAniSpeed);
	}
	return true;
}

//设置武器动作
void VisualComponentMZ::setWeaponAction()
{
	if( mModelNodeState.m_pBodyNode)
	{
		mChangePartManager.setWeaponAnimation( mModelNodeState.m_pBodyNode->getModelInstance()->getCurrentAnimation());
	}
}

//设置武器位置
void VisualComponentMZ::setWeaponPos()
{
	//当前的状态
	EVisualComponentState curState = m_stateMgr.GetCurrentState();

	//是否骑乘
	bool isMount = false;
	if(mModelNodeState.m_pRideNode)
		isMount = true;

	//设置武器的绑定位置
	if( mModelNodeState.m_curActionId == EntityAction_SitingDown 
		|| mModelNodeState.m_curActionId == EntityAction_Siting
		|| mModelNodeState.m_curActionId == EntityAction_Collect
		|| mModelNodeState.m_curActionId == EntityAction_JumpStart
		|| mModelNodeState.m_curActionId == EntityAction_JumpMidway
		|| mModelNodeState.m_curActionId == EntityAction_JumpEnd
		|| mModelNodeState.m_curActionId == EntityAction_Stun
		|| (mModelNodeState.m_curActionId == EntityAction_Stand && curState == EVCS_OnPeace)
		|| (mModelNodeState.m_curActionId == EntityAction_Walk && curState == EVCS_OnPeace)
		|| (mModelNodeState.m_curActionId == EntityAction_Run && curState == EVCS_OnPeace)
		|| isMount ) //武器放置在背后
	{
		mChangePartManager.setWeaponPos( ChangePartManager::WeaponPos_Back);
	}
	else//武器放置在手上
	{
		mChangePartManager.setWeaponPos( ChangePartManager::WeaponPos_Hands);
	}

}

void VisualComponentMZ::onRide(RideCallBackContext& context)
{
	//取消先前的链接
	if(	mModelNodeState.m_pCompositeNode )//设置callback
		mModelNodeState.m_pCompositeNode->getModelInstance()->setCallback(0);
	//getOwner()->onMessage(msgMoveSpeedChanged, m_ulTilePerTile);??
	if( mModelNodeState.m_pRideNode )
	{
		mModelNodeState.m_pRideNode->removeChild( mModelNodeState.m_pBodyNode);
		mModelNodeState.m_pRideNode = 0;
	}
	mModelNodeState.m_strBindPoint.clear();
	mModelNodeState.m_pCompositeNode = 0;
	
	//重新链接
	if( context.pRideNode )//骑乘
	{
		mModelNodeState.m_pRideNode = context.pRideNode;
		mModelNodeState.m_strBindPoint = context.strBindPoint;
		mModelNodeState.m_pRideNode->addChild( mModelNodeState.m_pBodyNode, mModelNodeState.m_strBindPoint.c_str() );
		mModelNodeState.m_pCompositeNode = mModelNodeState.m_pRideNode;

		// 去调用实体层的 一个函数；
		IPerson* pHero = gGlobalClient->getEntityClient()->GetHero();
		if (pHero && getOwner()->isMainPlayer() && pHero->GetCurState() == CREATURE_CONTROLSTATE_FLY)
		{
				pHero->OnResLoadedToMove();
			}

	}
	else //非骑乘
	{
		mModelNodeState.m_pRideNode = 0;
		mModelNodeState.m_strBindPoint.clear();
		mModelNodeState.m_pCompositeNode = mModelNodeState.m_pBodyNode;	
	}
	if( mModelNodeState.m_pCompositeNode) 
		mModelNodeState.m_pCompositeNode->getModelInstance()->setCallback(static_cast<IAnimationCallback*>(this) );


	//更新配置
	if( context.pRideNode)//骑乘
	{
		ConfigCreature* pRideConfig = ConfigCreatures::Instance()->getCreature(context.ulRideResId);		
		if( pRideConfig )
		{
			mModelNodeState.m_pRideConfig = pRideConfig;
			//调整包围盒的高度 = 宠物模型高度*3/4 + 人物模型高度的一半（取3/4是根据模型的不同，骑乘点离地面的高度也不同）	
			mShowRect.left = pRideConfig->boundingBox.left;
			mShowRect.right = pRideConfig->boundingBox.right-1;		
			LONG delta = 0;
			ConfigCreature* pBodyConfig = ConfigCreatures::Instance()->getCreature(getOwner()->getResId());
			if( pBodyConfig)
			{
				delta = (pBodyConfig->boundingBox.bottom - pBodyConfig->boundingBox.top) /2;
			}
			mShowRect.top = pRideConfig->boundingBox.bottom - ( pRideConfig->boundingBox.bottom - pRideConfig->boundingBox.top) *3.0f / 4.0f;
			mShowRect.top -= delta;
			mShowRect.bottom = pRideConfig->boundingBox.bottom -1;
		}
	}
	else//非骑乘
	{
		if( mModelNodeState.m_pConfig)
		{
			mShowRect.left = mModelNodeState.m_pConfig->boundingBox.left;
			mShowRect.right = mModelNodeState.m_pConfig->boundingBox.right-1;		
			mShowRect.top = mModelNodeState.m_pConfig->boundingBox.top;
			mShowRect.bottom = mModelNodeState.m_pConfig->boundingBox.bottom -1;
		}
	}


	//设置节点状态
	if( mModelNodeState.m_pCompositeNode)
	{
		//设置骑乘节点状态
		mModelNodeState.m_pCompositeNode->setPosition(getOwner()->getSpace());
		if( mModelNodeState.m_pRideNode )
		{
			if( mModelNodeState.m_pRideConfig)
			{
				mModelNodeState.m_pCompositeNode->setScale(mModelNodeState.m_pRideConfig->scale.x* mModelNodeState.m_fScale,
					mModelNodeState.m_pRideConfig->scale.y*mModelNodeState.m_fScale,
					mModelNodeState.m_pRideConfig->scale.z*mModelNodeState.m_fScale);
			}
		}
		else
		{
			if( mModelNodeState.m_pConfig)
			{
				mModelNodeState.m_pCompositeNode->setScale(mModelNodeState.m_pConfig->scale.x* mModelNodeState.m_fScale,
					mModelNodeState.m_pConfig->scale.y*mModelNodeState.m_fScale,
					mModelNodeState.m_pConfig->scale.z*mModelNodeState.m_fScale);
			}
		}
		Quaternion rot;
		rot.FromAngleAxis(mModelNodeState.m_lCurAngle,Vector3(0,1,0));
		mModelNodeState.m_pCompositeNode->setOrientation(rot);

		//骑乘状态,且两个节点都已经加载
		if( mModelNodeState.m_pRideNode && mModelNodeState.m_pBodyNode)
		{
			if( mModelNodeState.m_pConfig && mModelNodeState.m_pRideConfig )
			{
				float scale = mModelNodeState.m_pConfig->scale.x / mModelNodeState.m_pRideConfig->scale.x;
				mModelNodeState.m_pBodyNode->setScale( xs::Vector3::UNIT_SCALE * scale);
			}
			else
			{
				mModelNodeState.m_pBodyNode->setScale(xs::Vector3::UNIT_SCALE);
			}	
			mModelNodeState.m_pBodyNode->setPosition(xs::Vector3::ZERO);
			mModelNodeState.m_pBodyNode->setOrientation(xs::Quaternion::IDENTITY);
		}

		//设置动作
		setAnimate();
	}
}

void VisualComponentMZ::onChangePart(ChangePartCallBackContext & context)
{
	//取消先前的链接
	if(	mModelNodeState.m_pCompositeNode )//设置callback
		mModelNodeState.m_pCompositeNode->getModelInstance()->setCallback(0);
	//getOwner()->onMessage(msgMoveSpeedChanged, m_ulTilePerTile);??
	if( mModelNodeState.m_pRideNode)
	{
		mModelNodeState.m_pRideNode->removeChild(mModelNodeState.m_pBodyNode);
	}
	mModelNodeState.m_pBodyNode = 0;
	mModelNodeState.m_pCompositeNode = 0;

	//重新链接
	if( context.pNode)
	{
		mModelNodeState.m_pBodyNode = context.pNode;
		if( mModelNodeState.m_pRideNode)
		{
			mModelNodeState.m_pRideNode->addChild(mModelNodeState.m_pBodyNode, mModelNodeState.m_strBindPoint.c_str());
			mModelNodeState.m_pCompositeNode = mModelNodeState.m_pRideNode;
		}
		else
		{
			mModelNodeState.m_pCompositeNode = mModelNodeState.m_pBodyNode;
		}
	}
	else
	{
		mModelNodeState.m_pCompositeNode = mModelNodeState.m_pRideNode;		
	}

	if( mModelNodeState.m_pCompositeNode) 
		mModelNodeState.m_pCompositeNode->getModelInstance()->setCallback(static_cast<IAnimationCallback*>(this) );

	//更新配置
	if( mModelNodeState.m_pRideNode)//骑乘
	{	
		if( mModelNodeState.m_pRideConfig )
		{
			//调整包围盒的高度 = 宠物模型高度*3/4 + 人物模型高度的一半（取3/4是根据模型的不同，骑乘点离地面的高度也不同）	
			mShowRect.left = mModelNodeState.m_pRideConfig->boundingBox.left;
			mShowRect.right = mModelNodeState.m_pRideConfig->boundingBox.right-1;		
			LONG delta = 0;
			if( mModelNodeState.m_pConfig )
			{
				delta = (mModelNodeState.m_pConfig->boundingBox.bottom - mModelNodeState.m_pConfig->boundingBox.top) /2;
			}
			mShowRect.top = mModelNodeState.m_pRideConfig->boundingBox.bottom - 
				( mModelNodeState.m_pRideConfig->boundingBox.bottom - mModelNodeState.m_pRideConfig->boundingBox.top) *3.0f / 4.0f;
			mShowRect.top -= delta;
			mShowRect.bottom = mModelNodeState.m_pRideConfig->boundingBox.bottom -1;
		}
	}
	else//非骑乘
	{
		ConfigCreature* pBodyConfig = ConfigCreatures::Instance()->getCreature(mChangePartManager.getCurrentCreatureId() );
		if( pBodyConfig )
			mModelNodeState.m_pConfig = pBodyConfig;

		if( mModelNodeState.m_pConfig)
		{
			mShowRect.left = mModelNodeState.m_pConfig->boundingBox.left;
			mShowRect.right = mModelNodeState.m_pConfig->boundingBox.right-1;		
			mShowRect.top = mModelNodeState.m_pConfig->boundingBox.top;
			mShowRect.bottom = mModelNodeState.m_pConfig->boundingBox.bottom -1;
		}
	}

	//设置节点状态
	if( mModelNodeState.m_pCompositeNode)
	{
		//设置骑乘节点状态
		mModelNodeState.m_pCompositeNode->setPosition(getOwner()->getSpace());
		if( mModelNodeState.m_pRideNode )
		{
			if( mModelNodeState.m_pRideConfig)
			{
				mModelNodeState.m_pCompositeNode->setScale(mModelNodeState.m_pRideConfig->scale.x* mModelNodeState.m_fScale,
					mModelNodeState.m_pRideConfig->scale.y*mModelNodeState.m_fScale,
					mModelNodeState.m_pRideConfig->scale.z*mModelNodeState.m_fScale);
			}
		}
		else
		{
			if( mModelNodeState.m_pConfig)
			{
				mModelNodeState.m_pCompositeNode->setScale(mModelNodeState.m_pConfig->scale.x* mModelNodeState.m_fScale,
					mModelNodeState.m_pConfig->scale.y*mModelNodeState.m_fScale,
					mModelNodeState.m_pConfig->scale.z*mModelNodeState.m_fScale);
			}
		}
		Quaternion rot;
		rot.FromAngleAxis(mModelNodeState.m_lCurAngle,Vector3(0,1,0));
		mModelNodeState.m_pCompositeNode->setOrientation(rot);

		//骑乘状态,且两个节点都已经加载
		if( mModelNodeState.m_pRideNode && mModelNodeState.m_pBodyNode)
		{
			if( mModelNodeState.m_pConfig && mModelNodeState.m_pRideConfig )
			{
				float scale = mModelNodeState.m_pConfig->scale.x / mModelNodeState.m_pRideConfig->scale.x;
				mModelNodeState.m_pBodyNode->setScale( xs::Vector3::UNIT_SCALE * scale);
			}
			else
			{
				mModelNodeState.m_pBodyNode->setScale(xs::Vector3::UNIT_SCALE);
			}	
			mModelNodeState.m_pBodyNode->setPosition(xs::Vector3::ZERO);
			mModelNodeState.m_pBodyNode->setOrientation(xs::Quaternion::IDENTITY);	
		}

		//设置动作
		setAnimate();
	}
}

void VisualComponentMZ::onOneCycle(const std::string& animation,int loopTimes)
{
	//记录当前动作的循环次数
	mModelNodeState.m_curActionLoops = loopTimes;

	if (loopTimes == 0)
	{
		//const std::string oldAnimation = animation;
		//handleMessage(msgAniFinished, 0, 0);
		//if (oldAnimation == "SitdownStart")
		//{
		//	getOwner()->onCommand(EntityCommand_Siting);
		//}

		//by yhc
		//起跳和跳跃中不恢复到msgAniFinished,跳跃结束恢复到msgAniFinished
		if(animation == "JumpStart"||animation == "JumpStart1"||animation == "JumpMidway"||animation == "JumpMidway1")
		{
			return;
		}
		if (animation == "SitdownStart")
		{
			getOwner()->onCommand(EntityCommand_Siting);
		}
		else
		{
			handleMessage(msgAniFinished, 0, 0);
		}
	}
}

bool VisualComponentMZ::changePartAttchEffect( SPersonAttachPartChangeContext & context)
{
	////验证
	//if ( context.part >= static_cast<ulong>(EEntityPart_Max) ) return false;	
	//if ( !m_pMainNodeAppearance || m_pMainNodeAppearance->m_bPuppetInitialized == false ) 
	//{
	//	PartAttachEffectInfo * pInfo = new PartAttachEffectInfo(getOwner(),context);
	//	PartManager::getInstance().add( getOwner(), pInfo);
	//	return true;
	//}

	////是镶嵌宝石呢还是装备精炼
	//SPersonAttachPartChangeContext * pPreContext = 0;
	//if( context.attachType == EAttachType_SmeltSoul)
	//	pPreContext = &m_pMainNodeAppearance->m_arrPreviousSmeltEffect[context.part];
	//else if( context.attachType == EAttachType_EmbedGems)
	//	pPreContext = &m_pMainNodeAppearance->m_arrPreviousGemsEffect[context.part];
	//else return false;

	//if( m_pMainNodeAppearance->m_arrPreviousPart[context.part].perform == false )
	//{
	//	pPreContext->perform = false;
	//	return false;//没有装备
	//}

	//
	//bool bRet = false;

	//if( context.perform == false) //卸载装备附加特效
	//{
	//	if( pPreContext->perform == false ) return true;//已经卸载特效了
	//	if( context.part == EEntityPart_Suit_Armet || context.part == EEntityPart_Armet )
	//	{
	//		//普通头盔和时装头盔的卸载方式一致
	//		pPreContext->perform= false;
	//		uint index= 0;
	//		if( context.part == EEntityPart_Suit_Armet) index = ePuppetFashion;
	//		if( context.part == EEntityPart_Armet) index = ePuppetNormal;
	//		if( m_pMainNodeAppearance->m_canditate[index] == 0 ) return false;

	//		const char * pBP1 = getBindPoint( m_pMainNodeAppearance->m_arrPreviousPart[context.part].bindPoint[0] );
	//		ModelNode * pArmet = m_pMainNodeAppearance->m_canditate[index]->getFirstChildNodeByBone(pBP1);
	//		if( pArmet == 0 ) return false;
	//		for( uint i = 0; i<EBindResNum_Attachment; ++i)
	//		{
	//			const char * pBP2 = getBindPoint( pPreContext->bindPoint[i] );
	//			ModelNode * pnode = pArmet->getFirstChildNodeByBone(pBP2);
	//			if( pnode) pArmet->destroyChild(pnode);
	//		}
	//		bRet = true;
	//	}
	//	if( context.part == EEntityPart_Weapon )
	//	{
	//		pPreContext->perform = false;
	//		for( uint i=0; i < ePuppetMax; ++i)
	//		{
	//			if( m_pMainNodeAppearance->m_canditate[i] == 0 ) continue;
	//			for( uint j = 0; j <EBindResNum_MainPart; ++j)
	//			{
	//				const char * pBP1 = getBindPoint(m_pMainNodeAppearance->m_arrPreviousPart[context.part].bindPoint[j] );
	//				ModelNode * pWeapon = m_pMainNodeAppearance->m_canditate[i]->getFirstChildNodeByBone(pBP1);
	//				if( !pWeapon ) continue;
	//				for( uint k =0; k < EBindResNum_Attachment; ++k)
	//				{
	//					const char * pBP2 = getBindPoint( pPreContext->bindPoint[k] );
	//					ModelNode * pnode = pWeapon->getFirstChildNodeByBone(pBP2);
	//					if( pnode) pWeapon->destroyChild(pnode);
	//				}
	//			}
	//		}	
	//		bRet = true;
	//	}

	//	if( context.part == EEntityPart_Suit_Armor || context.part == EEntityPart_Armor )
	//	{	
	//		uint index = 0;
	//		if( context.part == EEntityPart_Suit_Armor ) index = ePuppetFashion;
	//		if( context.part == EEntityPart_Armor) index = ePuppetNormal;	
	//		pPreContext->perform = false;
	//		if( m_pMainNodeAppearance->m_canditate[index] == 0 ) return false;
	//		for( uint i=0; i < EBindResNum_Attachment; ++i)
	//		{
	//			const char * pBP = getBindPoint(pPreContext->bindPoint[i]);
	//			ModelNode * pnode = m_pMainNodeAppearance->m_canditate[index]->getFirstChildNodeByBone(pBP);
	//			if( pnode) m_pMainNodeAppearance->m_canditate[index]->destroyChild(pnode);			
	//		}
	//	}
	//	bRet = true;
	//}
	//else//附加装备附加特效
	//{
	//	if( context.part == EEntityPart_Suit_Armet || context.part == EEntityPart_Armet )
	//	{
	//		//普通头盔和时装头盔的卸载方式一致
	//		*pPreContext = context;
	//		uint index  =0;
	//		if( context.part == EEntityPart_Suit_Armet) index = ePuppetFashion;
	//		if( context.part == EEntityPart_Armet) index = ePuppetNormal;
	//		if( m_pMainNodeAppearance->m_canditate[index] == 0 ) return true;

	//		const char * pBP1 = getBindPoint( m_pMainNodeAppearance->m_arrPreviousPart[context.part].bindPoint[0]);
	//		ModelNode * pSuitArmet = m_pMainNodeAppearance->m_canditate[index]->getFirstChildNodeByBone(pBP1);
	//		if( pSuitArmet == 0 ) return false;

	//		for( uint i = 0; i <EBindResNum_Attachment; ++i)
	//		{
	//			const char * pBP2 = getBindPoint( context.bindPoint[i]);
	//			const std::string & filename = ConfigCreatureRes::Instance()->getResFromId(context.resId[i]);
	//			if( filename.empty() ) continue;
	//			ModelNode * pnode = ModelNodeCreater::create(filename.c_str() );
	//			if( !pnode) continue;
	//			bool ret = pSuitArmet->addChild(pnode,pBP2);
	//			if(!ret) { pnode->release(); continue; }
	//			ColorValue cl;
	//			cl.setAsARGB( context.color[i]);
	//			pnode->setGlobalDiffuse(cl);
	//			pnode->setScale(context.scale[i],context.scale[i], context.scale[i]);
	//		}

	//		bRet = true;
	//	}
	//	if( context.part == EEntityPart_Weapon )
	//	{
	//		*pPreContext = context;
	//		for( uint i=0; i < ePuppetMax; ++i)
	//		{
	//			if( m_pMainNodeAppearance->m_canditate[i] == 0 ) continue;
	//			for( uint j =0; j< EBindResNum_MainPart; ++j)
	//			{
	//				const char * pBP1 = getBindPoint( m_pMainNodeAppearance->m_arrPreviousPart[context.part].bindPoint[j] );
	//				ModelNode * pWeapon = m_pMainNodeAppearance->m_canditate[i]->getFirstChildNodeByBone(pBP1);
	//				if( !pWeapon ) continue;
	//				for( uint k =0; k < EBindResNum_Attachment; ++k)
	//				{
	//					const char * pBP2 = getBindPoint( context.bindPoint[k]);
	//					const std::string filename = ConfigCreatureRes::Instance()->getResFromId(context.resId[k]);
	//					if( filename.empty()) continue;
	//					ModelNode * pnode = ModelNodeCreater::create(filename.c_str());
	//					if(!pnode) continue;
	//					bool ret =  pWeapon->addChild(pnode,pBP2);
	//					if( !ret) { pnode->release(); continue;}
	//					ColorValue cl;
	//					cl.setAsARGB(context.color[k]);
	//					pnode->setGlobalDiffuse(cl);
	//					pnode->setScale( context.scale[k],context.scale[k],context.scale[k]);
	//				}
	//			}
	//		}
	//		bRet = true;
	//	}

	//	if( context.part == EEntityPart_Suit_Armor || context.part == EEntityPart_Armor )
	//	{	
	//		uint index = 0;
	//		if( context.part == EEntityPart_Suit_Armor ) index = ePuppetFashion;
	//		if( context.part == EEntityPart_Armor) index = ePuppetNormal;			
	//		*pPreContext = context;
	//		if( m_pMainNodeAppearance->m_canditate[index] == 0 ) return true;
	//		for( uint i=0; i < EBindResNum_Attachment; ++i)
	//		{
	//			const std::string filename = ConfigCreatureRes::Instance()->getResFromId(context.resId[i]);
	//			if( filename.empty() ) continue;
	//			ModelNode * pnode = ModelNodeCreater::create(filename.c_str());
	//			if( !pnode) continue;
	//			const char * pBP2 = getBindPoint(context.bindPoint[i]);
	//			bool ret =  m_pMainNodeAppearance->m_canditate[index]->addChild(pnode,pBP2);
	//			if( !ret) { pnode->release(); continue; }
	//			ColorValue cl;
	//			cl.setAsARGB(context.color[i]);
	//			pnode->setGlobalDiffuse(cl);
	//			pnode->setScale(context.scale[i],context.scale[i],context.scale[i]);
	//		}
	//		bRet = true;
	//	}	
	//}

	//if( bRet)
	//{
	//	if( context.attachType == EAttachType_SmeltSoul ) _changeUIAnimation(eChangeUIASmeltEffect,0,&context,0,0,false,0);
	//	if( context.attachType == EAttachType_EmbedGems ) _changeUIAnimation(eChangeUIAGemsEffect,0,0,&context,0,false,0);
	//	return true;
	//}
	//else return false;
	return false;
}
bool VisualComponentMZ::deformPart(SPersonPartDeformationContext & context)
{
	////验证
	//if ( context.part >= static_cast<ulong>(EEntityPart_Max) ) return false;	
	//if ( !m_pMainNodeAppearance || m_pMainNodeAppearance->m_bPuppetInitialized == false ) 
	//{
	//	DeformPartInfo * pInfo = new DeformPartInfo(getOwner(),context);
	//	PartManager::getInstance().add( getOwner(), pInfo);
	//	return true;
	//}

	////目前只对武器有效，其它的一概忽略
	//if(  m_pMainNodeAppearance->m_arrPreviousPart[context.part].perform == false)
	//{
	//	m_pMainNodeAppearance->m_arrPreviousPartDeform[context.part].deform = false;
	//	return false;
	//}

	//SPersonPartDeformationContext & preContext = m_pMainNodeAppearance->m_arrPreviousPartDeform[context.part];

	//if( !context.deform) //取消变形
	//{	
	//	if( preContext.deform == false) return false;

	//	if( context.part == EEntityPart_Weapon )
	//	{
	//		preContext = context;

	//		for( uint u=0;  u < ePuppetMax; ++u)
	//		{
	//			ModelNode * pmain = m_pMainNodeAppearance->m_canditate[u];
	//			if(pmain == 0 ) continue;
	//			ModelNode * pnode = 0;
	//			for( uint i=0; i <EBindResNum_MainPart;++i)
	//			{
	//				const std::string filename = ConfigCreatureRes::Instance()->getResFromId(
	//					m_pMainNodeAppearance->m_arrPreviousPart[EEntityPart_Weapon].resId[i] );
	//				if( filename.empty())continue;
	//				pnode = ModelNodeCreater::create(filename.c_str());
	//				if(pnode ==0 ) continue;
	//				const char * pBP = getBindPoint( m_pMainNodeAppearance->m_arrPreviousPart[EEntityPart_Weapon].bindPoint[i] );
	//				ModelNode * pold = pmain->getFirstChildNodeByBone(pBP);
	//				if( pold)
	//				{
	//					cloneModelNodeStates(pold,pnode);
	//					transferSubnodesForAttachNode(pold,pnode);
	//					pmain->destroyChild(pold);
	//					bool ret = pmain->addChild(pnode, pBP);
	//					if( !ret) pnode->release();
	//				}
	//				else
	//				{
	//					bool ret = pmain->addChild(pnode,pBP);
	//					if(!ret) pnode->release();
	//				}
	//			}
	//		}
	//		return true;			
	//	}
	//}
	//else //变形
	//{
	//	if( context.part == EEntityPart_Weapon)
	//	{
	//		preContext = context;

	//		for( uint u=0;  u < ePuppetMax; ++u)
	//		{
	//			ModelNode * pmain = m_pMainNodeAppearance->m_canditate[u];
	//			if(pmain == 0 ) continue;
	//			ModelNode * pnode = 0;
	//			for( uint i=0; i <EBindResNum_MainPart;++i)
	//			{
	//				const std::string filename = ConfigCreatureRes::Instance()->getResFromId(context.resId[i] );
	//				if( filename.empty())continue;
	//				pnode = ModelNodeCreater::create(filename.c_str());
	//				if(pnode ==0 ) continue;
	//				const char * pBP = getBindPoint( m_pMainNodeAppearance->m_arrPreviousPart[EEntityPart_Weapon].bindPoint[i] );
	//				ModelNode * pold = pmain->getFirstChildNodeByBone(pBP);
	//				if( pold)
	//				{
	//					cloneModelNodeStates(pold,pnode);
	//					transferSubnodesForAttachNode(pold,pnode);
	//					pmain->destroyChild(pold);
	//					bool ret = pmain->addChild(pnode, pBP);
	//					if( !ret) pnode->release();
	//				}
	//				else
	//				{
	//					bool ret = pmain->addChild(pnode,pBP);
	//					if(!ret) pnode->release();
	//				}
	//			}
	//		}
	//		return true;	
	//	}
	//}
	//return false;
	return false;
}


bool VisualComponentMZ::setAction(ulong actionId, int loops , ulong replaceactionid,bool ignoreactionlistsyn, bool bForceStop)
{
	bool bSuccess = true;
	int ret = 0;
	if(gGlobalClient->getSceneManager()->getRunType()==RUN_TYPE_GAME)
	{
		SEventAnimation animation;
		animation.nActionID = actionId;
		IEntity *pEntity = (IEntity*)getOwner()->getUserData();
		if (pEntity)
		{
			UID uid = pEntity->GetUID();
			int eventType = pEntity->GetEventSourceType();
			IEventEngine * pEventEngine = gGlobalClient->getEventEngine();
			if(pEventEngine != NULL)
			{
				bSuccess = pEventEngine->FireVote(EVENT_ACTION_ANIMATION,
					eventType,
					ANALYZEUID_SERIALNO(uid),
					(LPCSTR)&animation,
					sizeof(SEventAnimation)
					);
			}
		}
	}
	if (bSuccess)
	{
		ret = mActionList.setAction(actionId,loops,bForceStop);
	}
	else
	{
		return false;
	}


	//如果设置动作成功
	if( ret || (!ret && ignoreactionlistsyn ) )
	{
		//保存参数
		mModelNodeState.m_curActionId = actionId;
		mModelNodeState.m_curRepActionId = replaceactionid;
		mModelNodeState.m_curActionLoops = loops;

		//设置当前动作
		if (setAnimate())
		{
			//设置武器的动作
			setWeaponAction();

			//设置武器的位置
			setWeaponPos();
			return true;
		}
	}

	return false;
}

void VisualComponentMZ::resetMoveAction( bool ignoreactionlistsyn)
{
	ulong actionId = mIsRun ? EntityAction_Run : EntityAction_Walk;
	setAction(actionId, -1,0, ignoreactionlistsyn);
}

bool VisualComponentMZ::handleCommand(ulong cmd, ulong param1, ulong param2)
{
	//状态管理器根据命令来改变可视化组件的状态
	m_stateMgr.OnCommand( static_cast<EntityCommand>(cmd) );

	switch (cmd)
	{
	case EntityCommand_GetPart:
		{
			SEventPersonPerform_C * pC =reinterpret_cast<SEventPersonPerform_C*>(param2);
			return mChangePartManager.getPart(param1, *pC);
		};
		break;
	case EntityCommand_GetPartShow:
		{
			SEventPersonPerformShow_C* pC = reinterpret_cast<SEventPersonPerformShow_C*>(param2);
			return mChangePartManager.getPartShow(param1, *pC);
		}
		break;
	case EntityCommand_PartDeformation: //装备变形
		{
			SPersonPartDeformationContext * pcontext =reinterpret_cast<SPersonPartDeformationContext*>(param1); 
			if( !pcontext) return false;
			return deformPart(*pcontext);	
		}
		break;
	case EntityCommand_PartAttachEffect: //装备附加效果
		{
			SPersonAttachPartChangeContext * pcontext = reinterpret_cast<SPersonAttachPartChangeContext*>(param1);
			if( !pcontext) return false;
			return changePartAttchEffect(*pcontext);
		
		}
		break;
	case EntityCommand_ShowPart:
		{
			mChangePartManager.showPart(param1, param2 ? true:false);
			if( getOwner()->isMainPlayer() )
			{
				//发送装备显示事件
				SEventPersonPerformShow_C e;
				mChangePartManager.getPartShow(param1,e);
				gGlobalClient->getEventEngine()->FireExecute(EVENT_PERSON_PERFORM_SHOW, SOURCE_TYPE_PERSON, 0, (LPCSTR)&e, sizeof(e));
			}
		}
		break;
	case EntityCommand_SetPart:
		{
			SPersonMainPartChangeContext * context = reinterpret_cast<SPersonMainPartChangeContext*>(param1);
			if( context == 0 ) return false;
			mChangePartManager.changePart( *context);
			if( context->part == EEntityPart_Weapon )
			{
				//设置人物动作
				setAnimate();

				//设置武器动作
				setWeaponAction();

				//设置武器
				setWeaponPos();
			}

			if( getOwner()->isMainPlayer() )
			{
				//发送换装事件
				SEventPersonPerform_C e;
				mChangePartManager.getPart( context->part, e);
				gGlobalClient->getEventEngine()->FireExecute(EVENT_PERSON_PERFORM, SOURCE_TYPE_PERSON, 0, (LPCSTR)&e, sizeof(e));
			}
		}
		break;
	case EntityCommand_SetPartScale:
		{
			float scale = (float)param2;
			mChangePartManager.setPartScale(param1, scale);
		}
		break;
	case EntityCommand_SetPartColor:
		{
			const ColorValue* color = (const ColorValue*)param2;
			if( color )
				mChangePartManager.setPartColor(param1, *color);
		}
		break;
	case EntityCommand_AttachMagic:
		{
			MagicView* obj = (MagicView*)param1;
			if (obj)
			{
				obj->setEffectObserver(mAttachObjHolder.getHandle());
				obj->setTile(getOwner()->getTile());
				if (getOwner()->hasFlag(flagVisible))
				{
					obj->addFlag(flagVisible);
					obj->setScale(mModelNodeState.m_fScale);
					obj->onEnterViewport(true);
				}
				mAttachObjHolder.addAttachObj(obj);
				return true;
			}
			return false;
		}
		break;

    case EntityCommand_AddMagic:
        {
            MagicView* pAddedMagicView = (MagicView*)param1;
            if (pAddedMagicView != NULL)
            {
                mAttachObjHolder.AddRelativedObj(pAddedMagicView);
            }
        }
        break;

	case EntityCommand_DetachMagic:
		mAttachObjHolder.removeAttachObj(param1, (int)param2);
		break;

    case EntityCommand_RemoveMagic:
        {
            mAttachObjHolder.RemoveRelativedObj(param1, (int)param2);
        }
        break;

	case EntityCommand_ClearMagic:
		mAttachObjHolder.close();
		break;
	case EntityCommand_AddModifier:
		mModifierList.add((Modifier*)param1);
		break;
	case EntityCommand_RemoveModifiers:
		mModifierList.remove(param1);
		break;
	case EntityCommand_SetScale:
		{
			float old = mModelNodeState.m_fScale;
			mModelNodeState.m_fScale = *(const float*)param1;
			if (!Math::RealEqual(old, mModelNodeState.m_fScale))
			{
				//mAniSpeedMove *= old / mScale; //? 没有设置更新速度的选项
				mUpdateOption |= updateScale;
				mAttachObjHolder.setScale(mModelNodeState.m_fScale);
				//if (getOwner()->isMainPlayer())
				//Info("Scale: "<<_ff("%.2f", mScale)<<",AniSpeed: "<<_ff("%.2f", mAniSpeedMove)<<",TileMoveTime: "<<m_ulMoveSpeed<<endl);
			}
		}
		break;
	case EntityCommand_GetScale:
		*(float*)param1 = mModelNodeState.m_fScale;
		break;
	case EntityCommand_SetMoveSpeed:
		{
			if (param1 != mModelNodeState.m_ulMoveSpeed || mModelNodeState.m_bMoveStyleChange)
			{
				mModelNodeState.m_bMoveStyleChange = false;
				mModelNodeState.m_ulMoveSpeed = param1;
				ulong ulSpeed = mModelNodeState.m_ulMoveSpeed;

				if (!mIsRun)
				{
					if (ulSpeed > m_ulInitWalkSpeed)
					{
						ulSpeed = m_ulInitWalkSpeed;
					}
				}

				//设置移动速度
				// 如果是飞行状态下的；则改变移动速度，不改变动画播放速度；
				IEntity* pEntity = (IEntity*)getOwner()->getUserData();
				bool bFly = false;
				if (pEntity && pEntity->GetEntityClass()->IsPerson())
				{
					IPerson* pPerson = (IPerson*) pEntity;
					if (pPerson && pPerson->GetFLyState() == true)
					{
						bFly = true;
					}
				}
				// 飞行
				if (bFly)
				{
					setAniSpeed( (float) (EMOVESPEED_NORAML +150) / m_ulInitRunSpeed );
				}
				else
				{
					// 暂时注释，换一种表现
					//if (mIsRun)
					//{
					//	setAniSpeed( (float)ulSpeed / m_ulInitRunSpeed );
					//}
					//else
					//{
					//	setAniSpeed( (float)ulSpeed / m_ulInitWalkSpeed );
					//}

				}
				int nTimePerTile;

				/* 将移动速度转化为跨格时间
				实际跨格时间 = EMOVETIMEPERTILE_NORMAL * EMOVESPEED_NORAML  / 实际速度值
				*/
				if (ulSpeed > 0)
				{
					nTimePerTile = EMOVETIMEPERTILE_NORMAL * EMOVESPEED_NORAML  / ulSpeed;
				}
				else
				{
					nTimePerTile = EMOVETIMEPERTILE_MAX;
				}

				// 不能超过最大值和小于最小值
				nTimePerTile > EMOVETIMEPERTILE_MAX ? nTimePerTile = EMOVETIMEPERTILE_MAX : 0;
				nTimePerTile < EMOVETIMEPERTILE_MIN ? nTimePerTile = EMOVETIMEPERTILE_MIN : 0;

				mModelNodeState.m_ulTilePerTile = (ulong)nTimePerTile;
				//ulong moveSpeed = m_ulTilePerTile;//_32_X_sqrt_2 / m_ulTilePerTile; // 计算移动速度

				getOwner()->onMessage(msgMoveSpeedChanged, mModelNodeState.m_ulTilePerTile);
				//if (getOwner()->isMainPlayer())
				//Info("Scale: "<<_ff("%.2f", mScale)<<",AniSpeed: "<<_ff("%.2f", mAniSpeedMove)<<",TileMoveTime: "<<m_ulMoveSpeed<<",MoveSpeed: "<<_ff("%.2f", moveSpeed)<<endl);
			}
		}
		break;
	case EntityCommand_GetMoveSpeed:
		*(ulong*)param1 = mModelNodeState.m_ulMoveSpeed;
		break;
	case EntityCommand_SetColor:
		{
			//已经被换装管理器取代
			//mModelNodeState.m_vColor = *(const ColorValue*)param1;
			//mUpdateOption |= updateColor;
			const ColorValue * pColor = (const ColorValue*)param1;
			if( pColor)
			{
				mChangePartManager.setBodyColor(*pColor);
			}
		}
		break;
	case EntityCommand_GetColor:
		{
			//已经被换装管理器取代
			//*(ColorValue*)param1 = mModelNodeState.m_vColor;
			*(ColorValue*)param1 = mChangePartManager.getBodyColor();
		}
		break;
	case EntityCommand_SetMount:// TODO continue 这里处理骑马和下马的模型绑定过程，基类记录是否是骑马状态
		{
			mRideManager.ride(param1?true:false, param2);
		}
		break;

	case EntityCommand_GetMount:
		{
			*(ulong*)param1 = mRideManager.isRide() ? 1 : 0;
		}
		break;
	case EntityCommand_Stand: 
		{
			if (getOwner()->isMainPlayer())
			{
				breakable;
			}
			mModelNodeState.m_ulSitState= EN_SIT_NONE;

			//  原始代码；
			//param 1被用来指示如果处于站立状态是否重设动作
			if( param1 > 0) setAction(EntityAction_Stand,-1,0,true);
			else setAction(EntityAction_Stand, -1);
		}
		break;
	//case EntityCommand_ForceStop:
	//	if (getCurrentAction() == EntityAction_Skill_16)
	//	{
	//		setAction(EntityAction_Stand, -1, 0, true, true);
	//	}
	//	break;
	case EntityCommand_Fallow:
		{
			setAction(EntityAction_Stand_1, 1);
			//设置休闲音效(按概率触发)为30%
			int nRand = getRand(1,3);
			if (nRand == 2)
			{
				IFormManager* pFormMange = gGlobalClient->getFormManager();
				if (pFormMange) pFormMange->PlaySound(mModelNodeState.m_pConfig->aa.soundFallow,0,1,SOUNDRES_TYPE_SOUND);
			}
		}
		break;
	case EntityCommand_Move:
		{
			if( param1 > 0 )
				resetMoveAction( true);
			else
				resetMoveAction( false);
		}
		break;
	case EntityCommand_Jump:
		{
			//xs::Point ptSrc = *((xs::Point *)param1);
			//xs::Point ptDest = *((xs::Point *)param2);
			//m_pJumpManager = new JumpManager();
			//m_pJumpManager->create(ptSrc, ptDest);
			//m_pJumpManager->SetMoveSpeed(_32_X_sqrt_2 / mModelNodeState.m_ulTilePerTile);
			//mUpdateOption |= updateJump;

			if (param1)
			{
				if (m_pJumpManager) delete m_pJumpManager;
				m_pJumpManager = (JumpManager*)param1;
				m_pJumpManager->SetMoveSpeed(_32_X_sqrt_2 / mModelNodeState.m_ulTilePerTile);
				mUpdateOption |= updateJump;
			}
		}
		break;
	case EntityCommand_AttackReady:
	case EntityCommand_Attack:
		{
			if (getOwner()->isMainPlayer())
			{
				breakable;
			}
			ModelNode * pNode = getModelNode();
			if( !pNode) return false;
			const AttackContext* context = (const AttackContext*)param1;
			if( !context ) return false;

			//设置攻击动作
			bool ret = setAction(context->actionId,context->loops);
			if( !ret ) return false;
			setAniSpeed(context->fAnimateSpeed);

			//设置攻击音效(按概率触发)为30%
			int nRand = getRand(1,3);
			if (nRand == 2 && mModelNodeState.m_pConfig->aa.soundAttack != 0)
			{
				IFormManager* pFormMange = gGlobalClient->getFormManager();
				if (pFormMange) pFormMange->PlaySound(mModelNodeState.m_pConfig->aa.soundAttack,0,1,SOUNDRES_TYPE_SOUND);
			}
			return true;
			
			////设置攻击动作
			//const ActionInfo & curaction = mActionList.getCurActionInfo();
			//if( context->isSpellAttack )
			//{
			//	//改变了动作映射不用以前那一套
			//	//const char * animReplace = ( cmd ==EntityCommand_AttackReady)? 
			//	//	mConfig->aa.prepare.c_str(): mConfig->aa.spell.c_str();
			//	//const ActionInfo & curinfo = mActionList.getCurActionInfo();
			//	//setAction(curinfo.actionId,curinfo.loops, getActionId(animReplace) );//生物动作配置要改，改成动作id
			//	setAction(curaction.actionId,curaction.loops);
			//	setAniSpeed(context->fAnimateSpeed);
			//	return true;
			//}
			//else
			//{	
			//	//const ActionInfo & curinfo = mActionList.getCurActionInfo();
			//	//if( mConfig->aa.attacks.size() > 0 )
			//	//{
			//	//	int index = rand() % mConfig->aa.attacks.size();
			//	//	const char* animReplace = (cmd == EntityCommand_AttackReady) ? 
			//	//	mConfig->aa.prepare.c_str() : mConfig->aa.attacks[index].c_str();			
			//	//	//setAction( curinfo.actionId, curinfo.loops, getActionId(animReplace) ); //生物动作配置要改，改成动作id
			//	//}
			//	//else 
			//	//{
			//	//	setAction(curinfo.actionId,curinfo.loops);
			//	//}
			//	//改变了动作映射，不用以前那一套
			//	setAction(curaction.actionId,curaction.loops);
			//	setAniSpeed(context->fAnimateSpeed);
			//	return true;
			//}
		}
		break;
	case EntityCommand_Wound:
		{
			setAction(EntityAction_CombatWound, 1);
			//设置被击音效(按概率触发)为30%
			int nRand = getRand(1,3);
			if (nRand == 2)
			{
				IFormManager* pFormMange = gGlobalClient->getFormManager();
				if (pFormMange) pFormMange->PlaySound(mModelNodeState.m_pConfig->aa.soundWound,0,1,SOUNDRES_TYPE_SOUND);
			}
		}
		break;
	case EntityCommand_Death:
		{  //mAttachObjHolder.close();

		  ulong ulPetID = param2;

		  if (ulPetID != 0)
		  { 
			  setAction(EntityAction_Stand,-1);
			  setFadeDie();
			  mModelNodeState.m_bPetDie = (param1 == 0)?false:true;
		  }
		  else
		  {
			  //设置被击音效(按概率触发)为30%
			  int nRand = getRand(1,2);
			  if (nRand == 2)
			  {
				  IFormManager* pFormMange = gGlobalClient->getFormManager();
				  if (pFormMange) pFormMange->PlaySound(mModelNodeState.m_pConfig->aa.soundDie,0,1,SOUNDRES_TYPE_SOUND);
			  }
			 return setAction(EntityAction_Death, 1, EntityAction_Death);
		  }
		}
		break;
	case EntityCommand_Relive:
		{
			mActionList.clear();
			ulong ulPetID = param2;

			if (ulPetID != 0)
			{ 
				setAction(EntityAction_Stand,-1);
				setFadeIn();
				mModelNodeState.m_bPetDie= false;
			}
			else
			{
				setAction(EntityAction_Stand, -1);
			}
		}
		break;
	case EntityCommand_Stun:
		setAction(EntityAction_Stun, -1);
		break;
	case EntityCommand_Sitdown:
		{
			//if (m_ulSitState != EN_SIT_ACTION)
			{
				setAction(EntityAction_SitingDown, 1);
				mModelNodeState.m_ulSitState = EN_SIT_ACTION;
			}
		}
		break;
	case EntityCommand_Siting:
		{
			//if (m_ulSitState != EN_SIT_DONE)
			{
				setAction(EntityAction_Siting, -1);
				mModelNodeState.m_ulSitState = EN_SIT_DONE;
			}
		}
		break;
	case EntityCommand_GetSitState:
		{
			*(ulong*)param1 = mModelNodeState.m_ulSitState;
		}
		break;
	case EntityCommand_Collect:
		{
			setAction(EntityAction_Collect, 1);
		}
		break;
	case EntityCommand_ShowRibbon:
		{
			mChangePartManager.showRibbonSystem( param1 ? true:false);
		}
		break;
	case EntityCommand_RapidMoveStart:
		{
			if (param1)
			{
				if (mShadowManager) delete mShadowManager;
				mShadowManager = (ShadowManager*)param1;
				mShadowManager->setModelNode(getModelNode());
			}
		}
		break;
	case EntityCommand_RapidMoveEnd:
		{
			mShadowManager = 0;
			mAttachObjHolder.setPosition(getOwner()->getSpace());
		}
		break;
	case EntityCommand_ChangeModel:
		{
			//需要修改
			//ulong resId = (ulong)param1;
			//releaseRes();
			//getOwner()->setResId(resId);
			//requestRes(0);
			ulong resId = (ulong)param1;
			mChangePartManager.releaeRes();
			getOwner()->setResId(resId);
			mChangePartManager.requestRes(0);
		}
		break;
	case EntityCommand_GetAnimationTime:
		{
			int nTick = 0;
			ulong nActID = (ulong)param2;
			nTick = GetAnimationTime(nActID);
			*(int*)param1 = nTick;
		}
		 break;
	case EntityCommand_GetJumpTime:
		{
			int nTick = 0;
			xs::Point ptSrc = getOwner()->getTile();
			xs::Point ptTarget = *((xs::Point *)param2);
			nTick = GetJumpTime(ptSrc, ptTarget);
			*((int *)param1) = nTick;
		}
		break;
	case EntityCommand_UseBigTexture:
		{
			m_bUseBigTexture = true;
		};
		break;
	case EntityCommand_GetBoneSpace:
		{
			Vector3* v = (Vector3 *)param1;
			string s = *(string*)param2;
			GetBoneSpace(*v, s.c_str());
		}
		break;
	default:
		return VisualComponent::handleCommand(cmd, param1, param2);
	}
	return true;
}

void VisualComponentMZ::GetBoneSpace(Vector3& v, const char *p)
{
	ModelNode * pModelNode = mModelNodeState.m_pBodyNode;
	if(!pModelNode)
		return;

	ModelInstance * pModelInstance = pModelNode->getModelInstance();
	if(!pModelInstance)
		return;

	Bone *pBone = pModelInstance->getBone(p);
	if (!pBone)
		return;

	v = pModelNode->getFullTransform()*pBone->getTransformedPivot();
}

ulong VisualComponentMZ::getCurrentAction()
{
	return mModelNodeState.m_curActionId;
}

void VisualComponentMZ::handleMessage(ulong msgId, ulong param1, ulong param2)
{
	switch (msgId)
	{
	case msgTileChanged:
		{
			updateWorldCoord();
			getOwner()->setSortLeft(getOwner()->getWorld());
			getOwner()->setSortRight(getOwner()->getWorld());

			if (getOwner()->mCalcWorld)
			{
				Vector3 space;
				gGlobalClient->getSceneManager()->tile2Space(getOwner()->getTile(), space);
				getOwner()->setSpace(space);
			}
		}
		break;
	case msgPosChanged:
		{
			mAttachObjHolder.setPosition(getOwner()->getSpace());
		}
		break;
	case msgDirChanged:
		{
			mUpdateOption |= updateAngle;
		}
		break;
	case msgMoveStart:
		resetMoveAction();
		mActionList.setIsMoving(true);
		break;
	case msgMoveFinished:
		mActionList.setIsMoving(false);
		handleCommand(EntityCommand_Stand, 0, 0);
		break;	
	case msgAniFinished:
		{
			if( mActionList.hasNextAction() )
			{
				const ActionInfo& info = mActionList.nextAction();
				setAction(info.actionId,info.loops,0,true, true);
			}
			else
			{
				//不可能...
			}
		}
		break;
	case msgMoveStyleChanged:
		{

			mModelNodeState.m_bMoveStyleChange = true;
			handleCommand(EntityCommand_SetMoveSpeed, mModelNodeState.m_ulMoveSpeed, 0);

			if (mActionList.isMoving())
			{
				resetMoveAction();
			}
		}
		break;
	}

	VisualComponent::handleMessage(msgId, param1, param2);
}

const xs::Rect& VisualComponentMZ::getShowRect() const
{
	//在跳跃
	if(m_pJumpManager)
	{
		//ModelNode * pModelNode = mModelNodeState.m_pCompositeNode; //getModelNode();
		//if(!pModelNode)
		//	return mShowRect;

		//ModelInstance * pModelInstance = pModelNode->getModelInstance();
		//if(!pModelInstance)
		//	return mShowRect;

		//IModel * pIModel = pModelInstance->getModel();
		//if(!pIModel)
		//	return mShowRect;

		//CoreSkeleton * pCoreSkeleton = pIModel->getCoreSkeletion();
		//if(!pCoreSkeleton)
		//	return mShowRect;

		//CoreBone * pCoreBone = pCoreSkeleton->getCoreBone("bip01");//(ushort)0);
		//if(!pCoreBone)
		//	return mShowRect;

		//AnimationTime Time;
		//xs::Matrix4 mtxTransform;
		//xs::Quaternion  quatRotation;

		//JUMPSTATE jsnow = m_pJumpManager->GetJumpState();
		//uint nAniIndex = 0;

		//if(jsnow==JUMPSTATE_STATR)
		//	nAniIndex = EntityAction_JumpStart;
		//if(jsnow==JUMPSTATE_MIDWAY)
		//	nAniIndex = EntityAction_JumpMidway;
		//if(jsnow==JUMPSTATE_END)
		//	nAniIndex = EntityAction_JumpEnd;
		//
		////动作名字
		//ActionMapContext ridecontext;
		//ridecontext.iscreature = true;
		//ridecontext.ismount = false;
		//ridecontext.state = EVCS_OnPeace;
		//ridecontext.weapon = EWeapon_NoWeapon;
		//const char * pAction = ConfigActionMap::Instance()->getMappedAction(ridecontext,nAniIndex);
		//if(!pAction)
		//	return mShowRect;

		////起始位置
		//pCoreBone->getBoneData(&Time,-1,mtxTransform,quatRotation);
		//Vector3 v0 = mtxTransform.getTrans()*pModelNode->getScale();
		//xs::Point pt0;
		//v0 = gGlobalClient->getRenderSystem()->project(v0);
		//gGlobalClient->getSceneManager()->space2World(v0,pt0);

		//Animation * pAnimation = pIModel->hasAnimation(pAction);
		//if(!pAnimation)
		//	return mShowRect;

		//Time.start = pAnimation->getTimeStart();
		//Time.current = 0;//Time.start + m_pJumpManager->GetAniTime();
		//Time.end = pAnimation->getTimeEnd();

		////获取当前时间根骨胳(索引为零)的位置
		//pCoreBone->getBoneData(&Time,-1,mtxTransform,quatRotation);
		////Vector3 v1 = mtxTransform.getTrans()*pModelNode->getScale();

		//现在完全由程序控制高度
		/*Vector3 v1;
		v1.x = v1.y = v1.z = 0.0f;
		v1.y = m_pJumpManager->GetjumpHeight();
		v1 = gGlobalClient->getRenderSystem()->project(v1);
		Vector3 space = getOwner()->getSpace();
		
		xs::Point pt1;
		gGlobalClient->getSceneManager()->space2World(space+v1,pt1);*/



		float fHeight = m_pJumpManager->GetjumpHeight()*0.707; //0.707 ; 二分之根号二
		
		//偏移一下矩形
		static xs::Rect rc;
		::CopyRect(&rc,&mShowRect);
		rc.top -= fHeight;//-v0.y;
		rc.bottom -= fHeight;//-v0.y;
		//Info("升高"<<v1.y-v0.y<<endl);
		return rc;

		//return mShowRect;
	}
	else
	{
		//不在跳跃
		return mShowRect;
	}
}

void VisualComponentMZ::UseBigTexture()
{
	ModelNode * pModelNode = mModelNodeState.m_pCompositeNode; //getModelNode();
	if(!pModelNode)
		return ;

	ModelInstance * pModelInstance = pModelNode->getModelInstance();
	if(!pModelInstance)
		return ;

	//每个子模型都换成精度高的贴图
	SubModelInstance * pSubModelInstance = pModelInstance->getFirstSubModel();
	while(pSubModelInstance)
	{
		Material * pMaterial = pSubModelInstance->getMaterial();
		if(!pMaterial)
			return;
		MaterialLayer * pMaterialLayer = pMaterial->getLayer(0);
		
		if(pMaterialLayer)
		{
			std::string textureName = pMaterialLayer->m_szTexture;
			//去掉".dds"
			textureName = textureName.substr(0,textureName.size()-4);
			textureName += "_b.dds";

			xs::CStreamHelper data = xs::getFileSystem()->open(textureName.c_str());
			//文件不存在
			if(!data)
			{
				Error("未找到高精度纹理"<<textureName.c_str()<<endl);
				return;
			}

			pSubModelInstance->setTexture(0,textureName.c_str());
		}
		
		pSubModelInstance = pModelInstance->getNextSubModel();
	}

}
// 屏蔽周边玩家；
bool VisualComponentMZ::ShieldAroundPlayer()
{
	// 判断添加 屏蔽周围玩家接口；（WZH 2011.4.29）
	if (gGlobalClient->getSceneManager()->getRunType() == RUN_TYPE_GAME)
	{
		IConfigManager* pConfig = gGlobalClient->getConfigManager();
		IEntity* pEntity = (IEntity*)(getOwner()->getUserData());
		IPerson* pPerson = gGlobalClient->getEntityClient()->GetHero();
		if (pEntity && pPerson && pConfig && !pConfig->getSystemSetShowAllPerson())
		{
			// 如果是人 并且不是本人；就屏蔽掉；
			if (getOwner()->getEntityType() == typePerson && pEntity->GetUID() != pPerson->GetUID())
			{
				ITeamClient* pTeamClient = gGlobalClient->getTeamClient();
				if(pTeamClient)
				{
					DWORD dwPDBID = pEntity->GetNumProp(CREATURE_PROP_PDBID);
					if (!pTeamClient->IsTeammate(dwPDBID))
					{
						return true;
					}

				}
			}
		}
	}

	return false;
}