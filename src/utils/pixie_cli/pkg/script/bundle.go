package script

type pixieScript struct {
	Pxl       string `json:"pxl"`
	Vis       string `json:"vis"`
	Placement string `json:"placement"`
	ShortDoc  string `json:"ShortDoc"`
	LongDoc   string `json:"LongDoc"`
}

type bundle struct {
	Scripts map[string]*pixieScript `json:"scripts"`
}
