// ============================================================
//  Vue 3 — 刮刮乐在线版 (一段式: 购买即结算)
// ============================================================

const { createApp, ref, reactive, computed, onMounted, nextTick } = Vue;
const app = createApp({ setup() {
  const user = reactive({ loggedIn:false,id:0,username:'',balance:0,profit:0 });
  const auth = reactive({ username:'', password:'' });
  const loginMode = ref(true), authError = ref(''), loading = ref(true);
  const souvenirs = ref([0,0,0]);
  const cardTypes = ref([]);
  const cardColors = computed(()=>cardTypes.value.map(c=>c.color));
  const selectedCard = ref(-1);
  const scratching = ref(false), showResult = ref(false);
  const showRanking = ref(false), rankTab = ref('profit'), ranking = ref([]);
  const currentCard = reactive({ cardId:0,cardType:0,cardName:'',positions:0,totalPrize:0,cells:[] });
  const cellCanvases = ref([]), scratchActive = ref(false), revealPct = ref(0), showReturnBtn = ref(false);
  const cheatClicks = ref(0), cheatMsg = ref(''), cheatAmount = ref(0), cheating = ref(false);
  const toast = ref(''); let toastTimer=null;

  function fmtMoney(v) {
    if(!v&&v!==0)return'0'; v=Number(v);
    if(Math.abs(v)>=10000)return(v/10000).toFixed(1)+'万'; return String(v);
  }
  async function api(m,p,b){
    let o={method:m,headers:{}}; if(b){o.headers['Content-Type']='application/json';o.body=JSON.stringify(b);}
    let t=localStorage.getItem('token'); if(t)o.headers['Authorization']='Bearer '+t;
    try{let r=await fetch(p,o);return await r.json();}catch(e){showToast('网络错误');return null;}
  }
  function showToast(m){toast.value=m;if(toastTimer)clearTimeout(toastTimer);toastTimer=setTimeout(()=>toast.value='',2000);}

  // ── 认证 ──
  async function doAuth(){
    authError.value='';
    let d=await api('POST',loginMode.value?'/api/login':'/api/register',{username:auth.username,password:auth.password});
    if(d&&d.token){localStorage.setItem('token',d.token);Object.assign(user,d.user,{loggedIn:true});await fetchMe();}
    else if(d&&d.error)authError.value=d.error;
  }
  function logout(){localStorage.removeItem('token');user.loggedIn=false;scratching.value=false;showResult.value=false;}
  function doReturn(){showResult.value=false;scratching.value=false;fetchMe();}

  async function fetchMe(){
    let d=await api('GET','/api/me');
    if(d&&d.username){Object.assign(user,d,{loggedIn:true});souvenirs.value=d.souvenirs||[0,0,0];}
  }

  async function loadCardTypes(){
    let d=await api('GET','/api/cards/types');
    if(d&&Array.isArray(d))cardTypes.value=d;
    else cardTypes.value=[{name:'小试一手',cost:10000,positions:10,maxPrize:25,color:'#3C8C50'},{name:'人之常情',cost:20000,positions:15,maxPrize:50,color:'#3C64B4'},{name:'人，稿子，钻石',cost:50000,positions:20,maxPrize:100,color:'#B43C8C'}];
  }

  onMounted(async()=>{window.addEventListener('mouseup',stopScratch);await loadCardTypes();await fetchMe();loading.value=false;});

  // ── 购买 ──
  async function buyCard(){
    if(selectedCard.value<0)return;
    let d=await api('POST','/api/cards/buy',{typeIndex:selectedCard.value});
    if(!d||d.error){showToast(d?.error||'购买失败');return;}
    currentCard.cardId=Number(d.cardId);currentCard.cardType=Number(d.cardType);
    currentCard.cardName=d.cardName;currentCard.positions=Number(d.positions);
    currentCard.totalPrize=Number(d.totalPrize);
    currentCard.cells=d.cells.map(c=>({isWin:c.isWin===true||c.isWin==='true',prize:Number(c.prize||0),displayPrize:Number(c.displayPrize)}));
    user.balance=Number(d.balance);user.profit=Number(d.profit);
    souvenirs.value=d.souvenirs||souvenirs.value;
    scratching.value=true;revealPct.value=0;showReturnBtn.value=false;cellCanvases.value=[];
    await nextTick();initCellCanvases(currentCard.positions,cellCanvases.value);
  }

  // ── 刮涂层 ──
  function getScratchXY(e){let r=e.target.getBoundingClientRect();return{x:Math.round(e.clientX-r.left),y:Math.round(e.clientY-r.top)};}
  function startScratch(idx,e){if(e.button!==0)return;scratchActive.value=true;let{x,y}=getScratchXY(e);scratchAt(cellCanvases.value[idx],x,y,BRUSH_R);updateRevealPct();}
  function doScratch(idx,e){if(!scratchActive.value||e.buttons!==1)return;let{x,y}=getScratchXY(e);scratchAt(cellCanvases.value[idx],x,y,BRUSH_R);updateRevealPct();}
  function stopScratch(){let was=scratchActive.value;scratchActive.value=false;if(was&&revealPct.value>=95)finishScratch();}
  function startScratchTouch(idx,e){scratchActive.value=true;let t=e.touches[0],r=e.target.getBoundingClientRect();scratchAt(cellCanvases.value[idx],Math.round(t.clientX-r.left),Math.round(t.clientY-r.top),BRUSH_R);updateRevealPct();}
  function doScratchTouch(idx,e){if(!scratchActive.value)return;let t=e.touches[0],r=e.target.getBoundingClientRect();scratchAt(cellCanvases.value[idx],Math.round(t.clientX-r.left),Math.round(t.clientY-r.top),BRUSH_R);updateRevealPct();}
  function updateRevealPct(){let s=0;for(let i=0;i<currentCard.positions;i++)s+=getCellRevealPct(cellCanvases.value[i]);revealPct.value=(s/currentCard.positions)*100;}
  function revealAll(){for(let i=0;i<currentCard.positions;i++)scratchAll(cellCanvases.value[i]);revealPct.value=100;finishScratch();}
  function finishScratch(){showResult.value=true;fetchMe();}

  // ── 作弊 ──
  async function cheat(){
    if(cheating.value)return;cheating.value=true;cheatClicks.value++;
    let d=await api('POST','/api/cheat',{clicks:cheatClicks.value});
    if(d){cheatMsg.value=d.message;cheatAmount.value=d.amount;user.balance=Number(d.balance);}
    cheating.value=false;
  }

  // ── 排行榜 ──
  function openRanking(){showRanking.value=true;loadRanking(rankTab.value);}
  async function loadRanking(t){rankTab.value=t;let d=await api('GET','/api/ranking?type='+t);if(d&&d.ranking)ranking.value=d.ranking;}

  const gridStyle=computed(()=>{let r=Math.ceil(currentCard.positions/5);return{gridTemplateRows:`repeat(${r},${CELL_H}px)`};});

  return{user,auth,loginMode,authError,souvenirs,cardTypes,cardColors,selectedCard,scratching,showResult,showRanking,rankTab,ranking,currentCard,cellCanvases,revealPct,cheatClicks,cheatMsg,cheatAmount,cheating,toast,loading,gridStyle,fmtMoney,doAuth,logout,fetchMe,buyCard,startScratch,doScratch,stopScratch,startScratchTouch,doScratchTouch,revealAll,doReturn,showReturnBtn,cheat,openRanking,loadRanking};
}});
app.mount('#app');
