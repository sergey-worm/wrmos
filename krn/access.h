//##################################################################################################
//
//  Access permissions.
//
//##################################################################################################

#ifndef ACCESS_H
#define ACCESS_H

enum kacc_t
{
	Acc_kr  = 1 << 5,
	Acc_kw  = 1 << 4,
	Acc_kx  = 1 << 3,
	Acc_ur  = 1 << 2,
	Acc_uw  = 1 << 1,
	Acc_ux  = 1 << 0,

	// aliases 1
	Acc_krx     = Acc_kr  | Acc_kx,
	Acc_krw     = Acc_kr  | Acc_kw,
	Acc_krwx    = Acc_kr  | Acc_kw | Acc_kx,
	Acc_urx     = Acc_ur  | Acc_ux,
	Acc_urw     = Acc_ur  | Acc_uw,
	Acc_urwx    = Acc_ur  | Acc_uw | Acc_ux,
	Acc_krw_urw = Acc_krw | Acc_urw,
	Acc_kx_ux   = Acc_kx  | Acc_ux,
	Acc_kw_ur   = Acc_kw  | Acc_ur,

	// aliases 2
	Acc_ktext     = Acc_krx,     //
	Acc_krodata   = Acc_kr,      //
	Acc_kdata     = Acc_krw,     //
	Acc_kio       = Acc_krw,     // no-cache
	Acc_kkip      = Acc_krw,     //
	Acc_kutcb     = Acc_krw,     //
	Acc_kfull     = Acc_krwx,    //
	Acc_utext     = Acc_urx,     //
	Acc_urodata   = Acc_ur,      //
	Acc_udata     = Acc_urw,     //
	Acc_uio       = Acc_urw,     // no-cache
	Acc_ukip      = Acc_ur,      //
	Acc_utcb      = Acc_krw_urw, //
	Acc_ufull     = Acc_urwx,    //
	Acc_kudata    = Acc_krw_urw, //
	Acc_kutext    = Acc_kx_ux,   //
};

#endif // ACCESS_H
